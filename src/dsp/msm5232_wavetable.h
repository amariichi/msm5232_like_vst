#pragma once
#include <array>
#include <vector>

namespace msm5232 {

// 512-sample base wavetable; select grid by effective length (64/128/256)
constexpr int kTableSize = 512;
using Table = std::array<float, kTableSize>;

struct Tables {
    Tables();
    const Table& get(int mask /*1..15*/, bool quantized4, int effectiveLen) const; // wav1|wav2|wav4|wav8
    int baseLen() const { return kTableSize; }
private:
    // Three grid variants:
    // -off375:   x starts at -0.375;   64-grid  (half-cycle dx=0.25)
    // -off4375:  x starts at -0.4375;  128-grid (half-cycle dx=0.125)
    // -off46875: x starts at -0.46875; 256-grid (half-cycle dx=0.0625)
    std::array<Table, 16> tables_unquant_off375_{};  // index by mask 0..15 (0 unused)
    std::array<Table, 16> tables_quant4_off375_{};   // 4-bit quantized
    std::array<Table, 16> tables_unquant_off4375_{}; // index by mask 0..15 (0 unused)
    std::array<Table, 16> tables_quant4_off4375_{};  // 4-bit quantized
    std::array<Table, 16> tables_unquant_off46875_{}; // index by mask 0..15 (0 unused)
    std::array<Table, 16> tables_quant4_off46875_{};  // 4-bit quantized
};

// Utility to compute a single table for a given mask
// x_offset: start of fractional x grid per segment (e.g., -0.375 or -0.4375)
// group: number of base samples per x step (64:4, 128:2, 256:1 when kTableSize=512)
Table makeTable_with_offset(int mask, bool quantize4, float x_offset, int group);

}
