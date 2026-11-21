#pragma once
// Biquad-filter (RBJ Audio EQ Cookbook), transponerad direktform II.
// Koefficienter beräknas med set*() – billigt nog att göra per block på
// ljudtråden (inga allokeringar). Tillstånd per kanal.

#include "AudioBuffer.h"
#include <cmath>

namespace gfx {

struct BiquadCoeffs {
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;

    static constexpr double kPi = 3.14159265358979323846;

    static BiquadCoeffs lowpass(double sr, double hz, double q = 0.7071) {
        const double w = 2.0 * kPi * clampHz(hz, sr) / sr, c = std::cos(w), a = std::sin(w) / (2.0 * q);
        return norm((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + a, -2 * c, 1 - a);
    }
    static BiquadCoeffs highpass(double sr, double hz, double q = 0.7071) {
        const double w = 2.0 * kPi * clampHz(hz, sr) / sr, c = std::cos(w), a = std::sin(w) / (2.0 * q);
        return norm((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + a, -2 * c, 1 - a);
    }
    static BiquadCoeffs peak(double sr, double hz, double q, double gainDb) {
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * clampHz(hz, sr) / sr, c = std::cos(w), a = std::sin(w) / (2.0 * q);
        return norm(1 + a * A, -2 * c, 1 - a * A, 1 + a / A, -2 * c, 1 - a / A);
    }
    static BiquadCoeffs lowShelf(double sr, double hz, double gainDb, double q = 0.7071) {
        const double A = std::pow(10.0, gainDb / 40.0), sq = 2.0 * std::sqrt(A);
        const double w = 2.0 * kPi * clampHz(hz, sr) / sr, c = std::cos(w), a = std::sin(w) / (2.0 * q);
        return norm(A * ((A + 1) - (A - 1) * c + sq * a), 2 * A * ((A - 1) - (A + 1) * c),
                    A * ((A + 1) - (A - 1) * c - sq * a), (A + 1) + (A - 1) * c + sq * a,
                    -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq * a);
    }
    static BiquadCoeffs highShelf(double sr, double hz, double gainDb, double q = 0.7071) {
        const double A = std::pow(10.0, gainDb / 40.0), sq = 2.0 * std::sqrt(A);
        const double w = 2.0 * kPi * clampHz(hz, sr) / sr, c = std::cos(w), a = std::sin(w) / (2.0 * q);
        return norm(A * ((A + 1) + (A - 1) * c + sq * a), -2 * A * ((A - 1) + (A + 1) * c),
                    A * ((A + 1) + (A - 1) * c - sq * a), (A + 1) - (A - 1) * c + sq * a,
                    2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq * a);
    }

private:
    static double clampHz(double hz, double sr) { return std::fmin(std::fmax(hz, 5.0), sr * 0.49); }
    static BiquadCoeffs norm(double b0, double b1, double b2, double a0, double a1, double a2) {
        return { float(b0 / a0), float(b1 / a0), float(b2 / a0), float(a1 / a0), float(a2 / a0) };
    }
};

class Biquad {
public:
    void setCoeffs(const BiquadCoeffs& c) noexcept { c_ = c; }
    void reset() noexcept { for (auto& s : s_) s[0] = s[1] = 0.0f; }

    float processSample(int ch, float x) noexcept {
        auto& s = s_[ch];
        const float y = c_.b0 * x + s[0];
        s[0] = c_.b1 * x - c_.a1 * y + s[1];
        s[1] = c_.b2 * x - c_.a2 * y;
        return y;
    }

    void process(int ch, float* data, int n) noexcept {
        for (int i = 0; i < n; ++i) data[i] = processSample(ch, data[i]);
    }

private:
    BiquadCoeffs c_;
    float s_[kMaxChannels][2] {};
};

// Enpolig DC-spärr (högpass ~10 Hz). Behövs efter asymmetrisk klippning.
class DcBlocker {
public:
    void prepare(double sr, double hz = 10.0) { r_ = float(std::exp(-2.0 * 3.14159265358979 * hz / sr)); reset(); }
    void reset() noexcept { for (int c = 0; c < kMaxChannels; ++c) x1_[c] = y1_[c] = 0.0f; }
    void process(int ch, float* d, int n) noexcept {
        float x1 = x1_[ch], y1 = y1_[ch];
        for (int i = 0; i < n; ++i) { const float y = d[i] - x1 + r_ * y1; x1 = d[i]; y1 = y; d[i] = y; }
        x1_[ch] = x1; y1_[ch] = y1;
    }
private:
    float r_ = 0.999f, x1_[kMaxChannels] {}, y1_[kMaxChannels] {};
};

} // namespace gfx
