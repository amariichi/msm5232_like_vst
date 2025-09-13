#include "dsp/bandlimited.h"
#include <numeric>

namespace msm5232 {

static float compute_rms(const Table& t) {
    double acc = 0.0;
    for (float v : t) acc += double(v) * double(v);
    return (float)std::sqrt(acc / double(t.size()));
}

std::vector<int> make_harmonic_cuts(int bandsPerOctave) {
    if (bandsPerOctave < 1) bandsPerOctave = 1;
    // Allow denser mipmaps up to 8 bands per octave
    if (bandsPerOctave > 8) bandsPerOctave = 8;
    std::vector<int> cuts;
    // Start from 2 harmonics; stop at 256 (Nyquist for N=512)
    const float step = std::pow(2.0f, 1.0f / (float)bandsPerOctave);
    float cur = 2.0f;
    cuts.push_back(2);
    while (cur < 256.0f) {
        cur *= step;
        int c = (int)std::round(cur);
        if (c < 2) c = 2;
        if (c > 256) c = 256;
        if (cuts.empty() || c != cuts.back()) cuts.push_back(c);
        if (c >= 256) break;
    }
    // Ensure last is exactly 256
    if (cuts.back() != 256) cuts.push_back(256);
    // unique ascending
    cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());
    return cuts;
}

BLSet build_bandlimited_set(const Table& base, int bandsPerOctave, bool normalizeRMS) {
    const int N = (int)base.size();
    // Forward DFT (naive, one-time); store complex spectrum X[k]
    std::vector<std::complex<float>> X(N);
    const float twoPiOverN = 2.0f * 3.14159265358979323846f / (float)N;
    for (int k = 0; k < N; ++k) {
        std::complex<float> acc(0.0f, 0.0f);
        for (int n = 0; n < N; ++n) {
            float ang = -twoPiOverN * float(k * n);
            float cs = std::cos(ang);
            float sn = std::sin(ang);
            acc += std::complex<float>(cs, sn) * base[(size_t)n];
        }
        X[(size_t)k] = acc;
    }

    std::vector<int> cuts = make_harmonic_cuts(bandsPerOctave);
    BLSet set;
    set.hcuts = cuts;
    set.tables.resize(cuts.size());
    set.baseRMS = compute_rms(base);

    // For each cutoff, zero bins above cutoff while preserving conjugate symmetry
    for (size_t ci = 0; ci < cuts.size(); ++ci) {
        int H = cuts[ci];
        std::vector<std::complex<float>> Y = X; // copy
        // bins: 0..N-1, positive freqs 1..N/2-1, Nyquist at N/2
        int nyq = N / 2;
        // Apply soft taper near cutoff to reduce leakage/zipper in modulated cases
        const int taperBins = 6; // 4..8が目安
        if (H < nyq) {
            int startTaper = std::max(1, H - taperBins);
            for (int k = 1; k <= nyq; ++k) {
                if (k > H) {
                    Y[(size_t)k] = std::complex<float>(0.0f, 0.0f);
                    Y[(size_t)(N - k)] = std::complex<float>(0.0f, 0.0f);
                } else if (k >= startTaper) {
                    // raised-cosine from 1 -> 0 across [startTaper .. H]
                    float t = float(k - startTaper) / float(std::max(1, H - startTaper));
                    float w = 0.5f * (1.0f + std::cos(3.14159265358979323846f * t));
                    Y[(size_t)k] *= w;
                    Y[(size_t)(N - k)] *= w;
                }
            }
        } else {
            // H >= nyquist: keep all (no taper) to preserve maximum brightness
        }
        // Inverse DFT (naive)
        Table t{};
        for (int n = 0; n < N; ++n) {
            std::complex<float> acc(0.0f, 0.0f);
            for (int k = 0; k < N; ++k) {
                float ang = twoPiOverN * float(k * n);
                float cs = std::cos(ang);
                float sn = std::sin(ang);
                acc += std::complex<float>(cs, sn) * Y[(size_t)k];
            }
            // 1/N scaling for inverse DFT
            acc /= float(N);
            t[(size_t)n] = acc.real();
        }
        // Normalize to match base RMS or clamp peak
        if (normalizeRMS) {
            float r = compute_rms(t);
            if (r > 1e-12f) {
                float g = set.baseRMS / r;
                for (float& v : t) v *= g;
            }
        } else {
            // peak normalize to <= 1
            float m = 0.0f; for (float v : t) m = std::max(m, std::fabs(v));
            if (m > 1.0f) { for (float& v : t) v /= m; }
        }
        set.tables[ci] = t;
    }
    return set;
}

Table apply_lowpass_with_taper(const Table& base, int H, int taperBins, bool normalizeRMS) {
    const int N = (int)base.size();
    int nyq = N / 2;
    if (H < 1) H = 1; if (H > nyq) H = nyq;
    if (taperBins < 0) taperBins = 0;

    // DFT
    std::vector<std::complex<float>> X(N);
    const float twoPiOverN = 2.0f * 3.14159265358979323846f / (float)N;
    for (int k = 0; k < N; ++k) {
        std::complex<float> acc(0.0f, 0.0f);
        for (int n = 0; n < N; ++n) {
            float ang = -twoPiOverN * float(k * n);
            float cs = std::cos(ang);
            float sn = std::sin(ang);
            acc += std::complex<float>(cs, sn) * base[(size_t)n];
        }
        X[(size_t)k] = acc;
    }

    // Low-pass with taper
    std::vector<std::complex<float>> Y = X;
    if (H < nyq) {
        int startTaper = std::max(1, H - taperBins);
        for (int k = 1; k <= nyq; ++k) {
            if (k > H) {
                Y[(size_t)k] = std::complex<float>(0.0f, 0.0f);
                Y[(size_t)(N - k)] = std::complex<float>(0.0f, 0.0f);
            } else if (k >= startTaper) {
                float t = float(k - startTaper) / float(std::max(1, H - startTaper));
                float w = 0.5f * (1.0f + std::cos(3.14159265358979323846f * t));
                Y[(size_t)k] *= w;
                Y[(size_t)(N - k)] *= w;
            }
        }
    }

    // IDFT
    Table t{};
    for (int n = 0; n < N; ++n) {
        std::complex<float> acc(0.0f, 0.0f);
        for (int k = 0; k < N; ++k) {
            float ang = twoPiOverN * float(k * n);
            float cs = std::cos(ang);
            float sn = std::sin(ang);
            acc += std::complex<float>(cs, sn) * Y[(size_t)k];
        }
        acc /= float(N);
        t[(size_t)n] = acc.real();
    }
    if (normalizeRMS) {
        float rBase = compute_rms(base);
        float r = compute_rms(t);
        if (r > 1e-12f) {
            float g = rBase / r;
            for (float& v : t) v *= g;
        }
    }
    return t;
}

}
