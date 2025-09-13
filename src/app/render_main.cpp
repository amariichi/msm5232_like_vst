#include "dsp/synth.h"
#include <vector>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

using namespace msm5232;

static void write_u32_le(FILE* f, uint32_t v) { std::fwrite(&v, 1, 4, f); }
static void write_u16_le(FILE* f, uint16_t v) { std::fwrite(&v, 1, 2, f); }

static bool write_wav24(const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr) {
    if (L.size() != R.size()) return false;
    uint32_t frames = (uint32_t)L.size();
    uint16_t channels = 2;
    uint16_t bitsPerSample = 24;
    uint16_t blockAlign = channels * (bitsPerSample / 8);
    uint32_t byteRate = sr * blockAlign;
    uint32_t dataBytes = frames * blockAlign;

    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    std::fwrite("RIFF", 1, 4, f);
    write_u32_le(f, 36 + dataBytes);
    std::fwrite("WAVE", 1, 4, f);
    std::fwrite("fmt ", 1, 4, f);
    write_u32_le(f, 16);
    write_u16_le(f, 1); // PCM
    write_u16_le(f, channels);
    write_u32_le(f, sr);
    write_u32_le(f, byteRate);
    write_u16_le(f, blockAlign);
    write_u16_le(f, bitsPerSample);
    std::fwrite("data", 1, 4, f);
    write_u32_le(f, dataBytes);

    auto write24 = [&](float x) {
        float clamped = x;
        if (clamped > 0.999999f) clamped = 0.999999f;
        if (clamped < -0.999999f) clamped = -0.999999f;
        int32_t v = static_cast<int32_t>(clamped * 8388607.0f);
        unsigned char b[3];
        b[0] = (unsigned char)(v & 0xFF);
        b[1] = (unsigned char)((v >> 8) & 0xFF);
        b[2] = (unsigned char)((v >> 16) & 0xFF);
        std::fwrite(b, 1, 3, f);
    };

    for (uint32_t i = 0; i < frames; ++i) {
        write24(L[i]);
        write24(R[i]);
    }
    std::fclose(f);
    return true;
}

int main(int argc, char** argv) {
    int sr = 48000;
    float seconds = 4.0f;
    int tone = 15; // default all combined
    if (argc > 1) tone = std::atoi(argv[1]);
    if (tone < 1 || tone > 15) tone = 15;

    Synth synth;
    SynthParams p;
    p.toneMask = tone;
    p.polyphony = 32;
    // 補間なしテーブルは64/128をサポート（既定は128）
    p.tableLen = 128;
    p.quantize4 = true;
    p.adsr.attack = 0.01f;
    p.adsr.decay = 0.2f;
    p.adsr.sustain = 0.6f;
    p.adsr.release = 0.3f;
    p.gain = 0.3f;
    synth.setup((float)sr);
    synth.setParams(p);

    std::vector<int> notes = {60, 64, 67, 71, 74, 77, 81, 84};
    for (int n : notes) synth.noteOn(n, 100);

    int total = int(seconds * sr);
    std::vector<float> L(total), R(total);
    int offSample = int(2.0f * sr);
    for (int i = 0; i < total; i += 64) {
        int block = std::min(64, total - i);
        synth.process(L.data() + i, R.data() + i, block);
        if (i < offSample && i + 64 >= offSample) {
            for (int n : notes) synth.noteOff(n);
        }
    }

    write_wav24("render.wav", L, R, sr);
    return 0;
}
