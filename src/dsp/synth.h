#pragma once
#include "dsp/msm5232_wavetable.h"
#include "dsp/voice.h"
#include "dsp/bandlimited.h"
#include <array>
#include <cstdint>

namespace msm5232 {

struct SynthParams {
    int toneMask = 1; // 1..15
    ADSRParams adsr{};
    float gain = 0.5f;
    int polyphony = 32; // 1..32
    bool quantize4 = true;
    int tableLen = 128; // 64 or 128 effective length
    // Bandlimit quality: 0=off, 1..8 = bands per octave
    int blQuality = 0;
    // HQ Mode: 0=Off, 1=Auto2x (high f0/deep vib only), 2=Force2x, 3=Force4x, 4=Force8x
    int hqMode = 0;
    // Pre-HighCut mode: 0=Off, 1=Fixed, 2=ByMaxNote
    int preHighCutMode = 0;
    // When ByMaxNote: highest expected MIDI note (0..127). Default=64
    int preHighCutMaxNote = 64;
};

class Synth {
public:
    void setup(float sampleRate);
    void setParams(const SynthParams& p);
    void noteOn(int note, int vel);
    void noteOff(int note);
    void process(float* outL, float* outR, int frames);
    void setPitchBendSemis(float semis) { pitchBendSemis_ = semis; }
    void setDetuneSemis(float semis) { detuneSemis_ = semis; }
    void setVibratoDepthSemis(float semis) { vibratoDepthSemis_ = semis; }
    void setVibratoRateHz(float hz) { vibratoRateHz_ = hz; }
    // amt: noise amplitude ratio relative to |signal|, allowed 0..100 (1:100)
    void setNoiseAdd(float amt) {
        if (amt < 0.f) amt = 0.f; if (amt > 100.f) amt = 100.f; noiseAdd_ = amt;
    }
private:
    float sr_ = 48000.0f;
    Tables tables_{};
    const Table* current_ = nullptr;     // raw base from Tables
    const Table* effective_ = nullptr;   // points to raw or preCutBase_
    Table preCutBase_{};                 // if preHighCut=On
    bool preCutValid_ = false;
    // Bandlimited set for current table parameters (lazily built)
    BLSet blset_{};
    bool blsetValid_ = false;
    std::array<Voice, 32> voices_{};
    SynthParams params_{};
    int nextVoice_ = 0; // round-robin for stealing within polyphony
    float pitchBendSemis_ = 0.0f; // from MIDI PB
    float detuneSemis_ = 0.0f;    // from UI param
    float vibratoDepthSemis_ = 0.0f; // LFO depth in semis
    float vibratoRateHz_ = 5.0f;     // default vibrato rate
    float vibratoPhase_ = 0.0f;      // 0..2pi
    float noiseAdd_ = 0.0f;          // ratio 0..100 (0..10000%) additive noise (off by default)
    uint32_t rngState_ = 0x12345678u; // simple xorshift RNG state
};

}
