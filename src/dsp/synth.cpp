#include "dsp/synth.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr float kTwoPi = 6.28318530717958647692f;
}

namespace msm5232 {

static inline float midi_to_freq_int(int note) {
    return 440.0f * std::pow(2.0f, (note - 69) / 12.0f);
}

void Synth::setup(float sampleRate) {
    sr_ = sampleRate > 1.0f ? sampleRate : 48000.0f;
    current_ = &tables_.get(params_.toneMask, params_.quantize4, params_.tableLen);
    effective_ = current_;
    preCutValid_ = false;
    blsetValid_ = false;
    for (auto& v : voices_) {
        v.setSampleRate(sr_);
        v.setTable(effective_, params_.tableLen);
        v.setADSR(params_.adsr);
    }
    vibratoPhase_ = 0.0f;
    // Seed RNG in a simple reproducible way from sample rate
    rngState_ = static_cast<uint32_t>(sr_) ^ 0x9E3779B9u;
}

void Synth::setParams(const SynthParams& p) {
    // Determine what actually changed to avoid expensive rebuilds
    bool tableChanged = (p.toneMask != params_.toneMask) || (p.quantize4 != params_.quantize4) || (p.tableLen != params_.tableLen);
    bool adsrChanged = (p.adsr.attack != params_.adsr.attack) || (p.adsr.decay != params_.adsr.decay) ||
                       (p.adsr.sustain != params_.adsr.sustain) || (p.adsr.release != params_.adsr.release);
    bool blQualChanged = (p.blQuality != params_.blQuality);
    bool preHCChanged = (p.preHighCutMode != params_.preHighCutMode) || (p.preHighCutMaxNote != params_.preHighCutMaxNote);

    params_ = p;

    if (tableChanged) {
        current_ = &tables_.get(params_.toneMask, params_.quantize4, params_.tableLen);
        preCutValid_ = false;
    }
    if (adsrChanged) {
        for (auto& v : voices_) v.setADSR(params_.adsr);
    }
    // Prepare effective base (with optional pre-highcut)
    if (tableChanged || preHCChanged || !preCutValid_) {
        int mode = params_.preHighCutMode;
        if (mode == 0) {
            effective_ = current_;
            preCutValid_ = true;
        } else if (mode == 1) {
            // Fixed gentle cut at ~0.65 * Nyquist
            int nyq = msm5232::kTableSize / 2;
            int H = (int)std::round(0.65f * nyq);
            preCutBase_ = apply_lowpass_with_taper(*current_, H, 12, true);
            effective_ = &preCutBase_;
            preCutValid_ = true;
        } else { // 2 = ByMaxNote
            int nyq = msm5232::kTableSize / 2;
            float f0max = midi_to_freq_int(params_.preHighCutMaxNote);
            float guard = std::exp2(vibratoDepthSemis_ * (1.0f/12.0f)) * 1.05f;
            float allowedH = (f0max > 0.0f ? (sr_ * 0.5f) / (f0max * guard) : (float)nyq);
            int H = (int)std::floor(std::max(1.0f, std::min((float)nyq, allowedH)));
            // Use slightly wider taper when H is small
            int taper = (H < 16) ? 8 : 12;
            preCutBase_ = apply_lowpass_with_taper(*current_, H, taper, true);
            effective_ = &preCutBase_;
            preCutValid_ = true;
        }
        for (auto& v : voices_) v.setTable(effective_, params_.tableLen);
    }
    // Rebuild bandlimited set only if necessary and only when quality > 0 (based on effective base)
    if (tableChanged || blQualChanged || preHCChanged || !blsetValid_) {
        blsetValid_ = false;
        if (params_.blQuality > 0) {
            blset_ = build_bandlimited_set(*effective_, params_.blQuality, true);
            blsetValid_ = true;
        }
    }
}

void Synth::noteOn(int note, int vel) {
    // find free voice within current polyphony or steal via round-robin
    int limit = std::max(1, std::min(params_.polyphony, (int)voices_.size()));
    int idx = -1;
    for (int i = 0; i < limit; ++i) {
        if (!voices_[i].active()) { idx = i; break; }
    }
    if (idx < 0) { idx = nextVoice_ % limit; nextVoice_ = (nextVoice_ + 1) % limit; }
    voices_[idx].noteOn(note, vel);
}

void Synth::noteOff(int note) {
    for (auto& v : voices_) if (v.active() && v.note() == note) v.noteOff();
}

void Synth::process(float* outL, float* outR, int frames) {
    float lfoInc = kTwoPi * (vibratoRateHz_ / (sr_ > 0.f ? sr_ : 48000.f));
    // Additive noise ratio d (0..100) relative to |signal|
    float d = (noiseAdd_ > 0.f ? noiseAdd_ : 0.f);
    // Compensation to avoid clipping at s + d*|s| (<= (1+d))
    float comp = (d > 0.f) ? (1.0f / (1.0f + d)) : 1.0f;

    // Avoid doing heavy work in audio callback; BL tables are prepared in setParams
    
    const float nyquist = 0.5f * sr_;
    for (int n = 0; n < frames; ++n) {
        float lfo = std::sin(vibratoPhase_);
        // Use exp2f for cheaper pow2
        float semis = pitchBendSemis_ + detuneSemis_ + vibratoDepthSemis_ * lfo;
        float pitchRatio = std::exp2(semis * (1.0f/12.0f));
        float s = 0.0f;
        if (params_.blQuality <= 0) {
            for (auto& v : voices_) if (v.active()) s += v.render(pitchRatio);
        } else {
            // Compute a conservative guard factor from vibrato depth to keep sidebands under Nyquist
            float guard = std::exp2(vibratoDepthSemis_ * (1.0f/12.0f)) * 1.05f; // +5% safety
            for (auto& v : voices_) if (v.active()) {
                // Choose tables for the current pitch
                float f0 = v.baseFreq() * pitchRatio;
                float ef0 = f0 * guard; // guarded frequency estimate
                int ia=0, ib=0; float mix=0.0f;
                choose_tables_for_freq(blset_, ef0, sr_, ia, ib, mix);
                const Table* tA = (ia >= 0 ? &blset_.tables[(size_t)ia] : nullptr);
                const Table* tB = (ib >= 0 ? &blset_.tables[(size_t)ib] : tA);
                // HQ oversampling: auto (2x) when ef0 is high, or forced (2x/4x/8x)
                int os = 1;
                if (params_.hqMode == 2) os = 2;          // Force2x
                else if (params_.hqMode == 3) os = 4;     // Force4x
                else if (params_.hqMode == 4) os = 8;     // Force8x
                else if (params_.hqMode == 1) {
                    // Auto2x: 近傍のhcutが最上段に近い/境界に近い時に発火
                    int last = (int)blset_.hcuts.size() - 1;
                    float hlimit = (sr_ * 0.5f) / std::max(ef0, 1e-6f);
                    float hHi = (float)blset_.hcuts[ib];
                    bool nearTop = (ib >= last - 1) || ((hlimit - hHi) < 4.0f);
                    if (nearTop) os = 2;
                }
                if (os <= 1) {
                    s += v.renderFromTwoTables(tA, tB, mix, pitchRatio);
                } else {
                    float e;
                    if (!v.beginFrame(e)) continue;
                    // Configure per-voice FIR decimator for this OS
                    v.decim().configure(os);
                    const float invOS = 1.0f / float(os);
                    // Push OS subsamples into decimator delay line
                    for (int k = 0; k < os; ++k) {
                        float sk = v.sampleFromTwoTables(tA, tB, mix);
                        v.decim().push(sk);
                        v.advancePhaseScaled(pitchRatio, invOS);
                    }
                    float vlin = v.velocity();
                    float ydec = v.decim().output();
                    s += ydec * e * vlin;
                }
            }
        }
        s *= params_.gain;

        float y = s;
        if (d > 0.0f) {
            // xorshift32
            uint32_t x = rngState_;
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            rngState_ = x;
            // Convert to float in [-1, 1]
            float u01 = (float)(x) * 2.3283064365386963e-10f; // [0,1)
            float noise = u01 * 2.0f - 1.0f; // [-1,1)
            // Additive noise proportional to |s| ensures silence has no noise
            y = (s + d * std::fabs(s) * noise) * comp;
        }
        outL[n] = y;
        outR[n] = y;
        vibratoPhase_ += lfoInc;
        if (vibratoPhase_ > kTwoPi) vibratoPhase_ -= kTwoPi;
    }
}

}
