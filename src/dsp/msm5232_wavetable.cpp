#include "dsp/msm5232_wavetable.h"
#include <cmath>
#include <algorithm>

namespace msm5232 {

static inline float fast_exp(float x) { return std::exp(x); }

// Tanh-shaped step curve sampled at fractional x
static float tanh_shape(float x /* nominally around 0..15 */) {
    const float a = 3.0f;
    const float b = 6.4f / 15.0f;
    const float num = (fast_exp(a - b * x) - 1.0f);
    const float den = (fast_exp(a - b * x) + 1.0f);
    const float norm = ( (fast_exp(a) - 1.0f) / (fast_exp(a) + 1.0f) );
    return ((num / den) / norm + 1.0f) * 100.0f;
}

static inline int bit(int mask, int n) { return (mask & (1 << n)) ? 1 : 0; }

// Build a 256-sample table using a fractional x-offset per segment and
// group size to control sampling interval:
//   group=2, x_offset=-0.375   => 64 grid (half-cycle dx=0.25)
//   group=1, x_offset=-0.4375  => 128 grid (half-cycle dx=0.125)
Table makeTable_with_offset(int mask, bool quantize4, float x_offset, int group) {
    Table y{};
    constexpr int N = kTableSize; // 512
    const int half = N / 2;           // 256 (wav1 segment)
    const int quarter = N / 4;        // 128 (wav2 segment)
    const int eighth = N / 8;         // 64  (wav4 segment)
    const int sixteenth = N / 16;     // 32  (wav8 segment)

    // x-step per sample over a half-cycle with grouping.
    // With half=256, dx_step = (16*group)/256 = group/16 => 0.0625 (g=1), 0.125 (g=2), 0.25 (g=4)
    const float dx_step = (16.0f * float(group)) / float(half);

    for (int k = 0; k < N; ++k) {
        float v = 0.0f;

        // wav1: 16 steps across half period, sign flip between halves
        if (bit(mask, 0)) {
            int posInHalf = k % half;
            // Grouping duplicates same x across consecutive samples (64:2 samples, 128:1)
            float xx = x_offset + dx_step * float(posInHalf / group);
            int sign = (k < half) ? +1 : -1;
            v += sign * tanh_shape(xx) * 1.0f;
        }
        // wav2: 8 steps across each quarter, sign flips every quarter: - + - +
        if (bit(mask, 1)) {
            int posInQuarter = k % quarter;
            float xx = x_offset + dx_step * float(posInQuarter / group); // ~0..7 range
            int qBlock = (k / quarter) % 2; // 0,1,0,1...
            int sign = (qBlock == 0) ? -1 : +1; // start negative
            v += sign * tanh_shape(xx) * 0.6f;
        }
        // wav4: 4 steps across each eighth, sign flips every eighth: - + - + ...
        if (bit(mask, 2)) {
            int posInEighth = k % eighth;
            float xx = x_offset + dx_step * float(posInEighth / group);
            int eBlock = (k / eighth) % 2;
            int sign = (eBlock == 0) ? -1 : +1;
            v += sign * tanh_shape(xx) * 0.5f;
        }
        // wav8: 2 steps across each sixteenth, sign flips every sixteenth: - + - + ...
        if (bit(mask, 3)) {
            int posInSixteenth = k % sixteenth;
            float xx = x_offset + dx_step * float(posInSixteenth / group);
            int sBlock = (k / sixteenth) % 2;
            int sign = (sBlock == 0) ? -1 : +1;
            v += sign * tanh_shape(xx) * 0.45f;
        }
        y[k] = v;
    }

    // Normalize to [-1,1]
    float maxAbs = 0.0f;
    for (float v : y) maxAbs = std::max(maxAbs, std::abs(v));
    if (maxAbs > 0.0f) {
        for (float& v : y) v = v / maxAbs;
    }
    // Optional 4-bit quantization (symmetric, -7..7 mapped to [-1,1])
    if (quantize4) {
        for (float& v : y) {
            float q = std::round(v * 7.0f) / 7.0f;
            if (q > 1.0f) q = 1.0f; if (q < -1.0f) q = -1.0f;
            v = q;
        }
    }
    return y;
}

Tables::Tables() {
    for (int m = 1; m <= 15; ++m) {
        // 64-grid:  x_offset=-0.375,   group=4 (dx=0.25)
        tables_unquant_off375_[m]    = makeTable_with_offset(m, false, -0.375f,   4);
        tables_quant4_off375_[m]     = makeTable_with_offset(m, true,  -0.375f,   4);
        // 128-grid: x_offset=-0.4375,  group=2 (dx=0.125)
        tables_unquant_off4375_[m]   = makeTable_with_offset(m, false, -0.4375f,  2);
        tables_quant4_off4375_[m]    = makeTable_with_offset(m, true,  -0.4375f,  2);
        // 256-grid: x_offset=-0.46875, group=1 (dx=0.0625)
        tables_unquant_off46875_[m]  = makeTable_with_offset(m, false, -0.46875f, 1);
        tables_quant4_off46875_[m]   = makeTable_with_offset(m, true,  -0.46875f, 1);
    }
}

const Table& Tables::get(int mask, bool quantized4, int effectiveLen) const {
    if (mask < 1) mask = 1; if (mask > 15) mask = 15;
    // Choose grid variant based on requested effective length
    if (effectiveLen <= 64) {
        return quantized4 ? tables_quant4_off375_[mask] : tables_unquant_off375_[mask];
    } else if (effectiveLen <= 128) {
        return quantized4 ? tables_quant4_off4375_[mask] : tables_unquant_off4375_[mask];
    } else { // 256+
        return quantized4 ? tables_quant4_off46875_[mask] : tables_unquant_off46875_[mask];
    }
}

}
