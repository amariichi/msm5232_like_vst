#pragma once
#include "dsp/msm5232_wavetable.h"
#include "dsp/adsr.h"
#include <cstdint>
#include <array>
#include <algorithm>
#include <cmath>

namespace msm5232 {

struct NoteEvent {
    int type; // 0=off, 1=on
    int note; // 0..127
    int velocity; // 0..127
};

class Voice {
public:
    void setSampleRate(float sr) { sr_ = sr; env_.setSampleRate(sr); }
    void setTable(const Table* t, int effectiveLen) { table_ = t; len_ = effectiveLen; }
    void setADSR(const ADSRParams& p) { env_.set(p); }
    void noteOn(int note, int vel);
    void noteOff();
    bool active() const { return active_; }
    int note() const { return note_; }
    float render(float pitchRatio);
    // Render using external table(s) chosen per-sample (bandlimited sets)
    // tables: array of pointers to tables (all length kTableSize)
    // num: number of tables
    // idxA/idxB: chosen adjacent table indices and crossfade 0..1 between them
    float renderFromTwoTables(const Table* tblA, const Table* tblB, float mix, float pitchRatio);
    float baseFreq() const { return baseFreq_; }
    float velocity() const { return velocity_; }
    // HQ oversampling helpers (process envelope once, sample/advance phase manually)
    bool beginFrame(float& eOut);
    float sampleFromTwoTables(const Table* tblA, const Table* tblB, float mix) const;
    void advancePhaseScaled(float pitchRatio, float invOversample);
    // Simple FIR decimator for internal oversampling (per-voice state)
    struct DecimFIR {
        int os = 1;              // decimation factor (1/2/4/8)
        int tapsN = 0;           // number of FIR taps (odd)
        int idx = 0;             // write index into delay line
        std::array<float, 128> z{}; // delay line for oversampled input (enough for taps up to 128)
        std::array<float, 128> h{}; // taps
        void reset() { idx = 0; std::fill(z.begin(), z.end(), 0.0f); }
        static float sinc(float x) { return (std::fabs(x) < 1e-6f) ? 1.0f : std::sin(x)/x; }
        static void makeLowpass(int L, float fc, std::array<float,128>& out) {
            // Hamming windowed-sinc, normalized to DC gain 1.0
            int M = L - 1; int mid = M / 2; const float twoPi = 6.28318530717958647692f;
            float sum = 0.0f;
            for (int n = 0; n < L; ++n) {
                float w = 0.54f - 0.46f * std::cos(twoPi * n / float(M));
                float x = twoPi * fc * (float(n - mid));
                float hn = 2.0f * fc * sinc(x);
                out[(size_t)n] = w * hn;
                sum += out[(size_t)n];
            }
            if (std::fabs(sum) > 1e-9f) {
                float g = 1.0f / sum; for (int n = 0; n < L; ++n) out[(size_t)n] *= g;
            }
        }
        void configure(int newOS) {
            if (newOS < 1) newOS = 1; if (newOS > 8) newOS = 8;
            if (newOS == os && tapsN > 0) return;
            os = newOS; reset();
            // Modest tap lengths per OS (odd, linear phase)
            int L = (os == 2 ? 17 : os == 4 ? 25 : os == 8 ? 33 : 1);
            tapsN = L;
            float fc = 0.45f / float(os); // normalized cutoff (Nyquist guard)
            makeLowpass(L, fc, h);
        }
        inline void push(float x) {
            if (tapsN <= 0) return;
            idx = (idx + 1) & 127; // wrap (size 128)
            z[(size_t)idx] = x;
        }
        inline float output() const {
            if (tapsN <= 0) return z[(size_t)idx];
            float acc = 0.0f;
            int j = idx;
            for (int n = 0; n < tapsN; ++n) {
                acc += h[(size_t)n] * z[(size_t)j];
                // manual wrap backward
                if (--j < 0) j = 127;
            }
            return acc;
        }
    };
    DecimFIR& decim() { return decim_; }
private:
    float sr_ = 48000.0f;
    const Table* table_ = nullptr;
    ADSR env_{};
    int note_ = -1;
    float velocity_ = 0.0f;
    float phase_ = 0.0f; // 0..32
    float baseInc_ = 0.0f; // increment at ratio=1.0
    bool active_ = false;
    int len_ = kTableSize; // effective table length (64/128/256)
    float baseFreq_ = 440.0f;
    DecimFIR decim_{};
};

}
