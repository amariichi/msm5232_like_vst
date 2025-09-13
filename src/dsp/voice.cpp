#include "dsp/voice.h"
#include <cmath>

namespace msm5232 {

static inline float midi_to_freq(int note) {
    return 440.0f * std::pow(2.0f, (note - 69) / 12.0f);
}

void Voice::noteOn(int n, int vel) {
    note_ = n;
    velocity_ = std::max(0, std::min(127, vel)) / 127.0f;
    float f = midi_to_freq(note_);
    baseFreq_ = f;
    baseInc_ = (float)len_ * f / sr_;
    phase_ = 0.0f;
    env_.gate(true);
    active_ = true;
    decim_.reset();
}

void Voice::noteOff() {
    env_.gate(false);
}

float Voice::render(float pitchRatio) {
    if (!table_ || !active_) return 0.0f;
    float e = env_.process();
    if (!env_.isActive() && e <= 0.0f) { active_ = false; return 0.0f; }

    int iBase = static_cast<int>(phase_);
    int stride = msm5232::kTableSize / len_; // e.g., 4 for 32, 2 for 64, 1 for 128
    int maskN = msm5232::kTableSize - 1;
    int idx0 = (iBase * stride) & maskN;           // current sample
    float s = (*table_)[idx0]; // direct lookup, no cubic interpolation

    float inc = baseInc_ * (pitchRatio > 0.f ? pitchRatio : 0.f);
    phase_ += inc;
    if (phase_ >= (float)len_) phase_ -= (float)len_;
    return s * e * velocity_;
}

float Voice::renderFromTwoTables(const Table* tblA, const Table* tblB, float mix, float pitchRatio) {
    if ((!tblA && !tblB) || !active_) return 0.0f;
    float e = env_.process();
    if (!env_.isActive() && e <= 0.0f) { active_ = false; return 0.0f; }

    int iBase = static_cast<int>(phase_);
    int stride = msm5232::kTableSize / len_;
    int maskN = msm5232::kTableSize - 1;
    int idx0 = (iBase * stride) & maskN;
    float sA = tblA ? (*tblA)[idx0] : 0.0f;
    float sB = tblB ? (*tblB)[idx0] : 0.0f;
    // simple linear crossfade between two band tables
    float s = sA * (1.0f - mix) + sB * mix;

    float inc = baseInc_ * (pitchRatio > 0.f ? pitchRatio : 0.f);
    phase_ += inc;
    if (phase_ >= (float)len_) phase_ -= (float)len_;
    return s * e * velocity_;
}

bool Voice::beginFrame(float& eOut) {
    if (!active_) { eOut = 0.0f; return false; }
    float e = env_.process();
    if (!env_.isActive() && e <= 0.0f) { active_ = false; eOut = 0.0f; return false; }
    eOut = e;
    return true;
}

float Voice::sampleFromTwoTables(const Table* tblA, const Table* tblB, float mix) const {
    int iBase = static_cast<int>(phase_);
    int stride = msm5232::kTableSize / len_;
    int maskN = msm5232::kTableSize - 1;
    int idx0 = (iBase * stride) & maskN;
    float sA = tblA ? (*tblA)[idx0] : 0.0f;
    float sB = tblB ? (*tblB)[idx0] : 0.0f;
    return sA * (1.0f - mix) + sB * mix;
}

void Voice::advancePhaseScaled(float pitchRatio, float invOversample) {
    float inc = baseInc_ * (pitchRatio > 0.f ? pitchRatio : 0.f) * invOversample;
    phase_ += inc;
    if (phase_ >= (float)len_) phase_ -= (float)len_;
}

}
