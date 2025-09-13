#pragma once
#include <vector>
#include <array>
#include <complex>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include "dsp/msm5232_wavetable.h"

namespace msm5232 {

struct BLSet {
    // Tables with increasing harmonic cutoff (e.g., 2,4,8,...,256)
    std::vector<Table> tables;
    std::vector<int> hcuts; // same length as tables
    float baseRMS = 1.0f;   // RMS of original base table for normalization
};

// Build a vector of harmonic cutoffs for bpo = bands per octave.
// Always returns unique ascending values in [2, 256], inclusive.
std::vector<int> make_harmonic_cuts(int bandsPerOctave);

// Build bandlimited tables ("mipmap") from a base 512-sample table.
// - bandsPerOctave: 1=low, 2=medium, 3..8=higher density.
// - normalizeRMS: if true, each table is scaled to match the base table RMS.
BLSet build_bandlimited_set(const Table& base, int bandsPerOctave, bool normalizeRMS=true);

// Choose two adjacent tables and crossfade factor given f0 and sampleRate.
// Returns indices (ia, ib) into set and mix [0..1] such that output = (1-mix)*ia + mix*ib.
// If only one table available, ia==ib and mix==0.
inline void choose_tables_for_freq(const BLSet& set, float f0, float sampleRate, int& ia, int& ib, float& mix) {
    if (set.tables.empty()) { ia = ib = -1; mix = 0.0f; return; }
    // Allowed harmonics at f0
    float hlimit = (f0 > 0.0f ? (sampleRate * 0.5f) / f0 : (float)set.hcuts.back());
    // clamp to range
    if (hlimit <= (float)set.hcuts.front()) { ia = 0; ib = 0; mix = 0.0f; return; }
    if (hlimit >= (float)set.hcuts.back()) { ia = (int)set.hcuts.size()-1; ib = ia; mix = 0.0f; return; }
    // Binary search (lower_bound)
    int lo = 0;
    int hi = (int)set.hcuts.size() - 1;
    while (hi - lo > 1) {
        int mid = (lo + hi) >> 1;
        if ((float)set.hcuts[mid] < hlimit) lo = mid; else hi = mid;
    }
    ia = lo; ib = hi;
    float hLo = (float)set.hcuts[lo];
    float hHi = (float)set.hcuts[hi];
    // Linear mix in harmonic domain (cheaper than log2)
    float t = (hlimit - hLo) / (hHi - hLo);
    if (t < 0.0f) t = 0.0f; if (t > 1.0f) t = 1.0f;
    // light easing
    mix = t * t * (3.0f - 2.0f * t);
}

// Apply a global soft low-pass with raised-cosine taper near H (harmonic index cutoff).
// H: keep up to H, taper over [H - taperBins .. H], zero above H. Preserves RMS when normalizeRMS=true.
Table apply_lowpass_with_taper(const Table& base, int H, int taperBins, bool normalizeRMS=true);

}
