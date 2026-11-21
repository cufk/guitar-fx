#pragma once
// 4x oversampling med två kaskadkopplade halvbands-FIR-steg (2x + 2x).
//
// Varför: en waveshaper (distortion) skapar övertoner långt över Nyquist.
// Utan oversampling viks de ner som oharmoniska, "digitala" toner (aliasing).
//
// Halvbandsfilter: varannan koefficient är noll och mitten är exakt 0,5.
// Uppsamplingens ena fas blir därför en ren fördröjning och den andra en FIR
// med (N+1)/2 koefficienter – billigt nog för iPad och Raspberry Pi.
//
// Linjär fas => konstant latens. latencySamples() rapporterar avrundat värde
// (exakt värde är c1 - 0,5 + (2*c2 - 1)/4 samples vid bas-frekvensen).

#include <algorithm>
#include <cmath>
#include <vector>

namespace gfx {

namespace detail {

// Kaiser-fönstrad halvbands-lågpass. c = mittindex (måste vara udda),
// längd N = 2c + 1. Returnerar koefficienterna för de jämna indexen
// h[0], h[2], ..., h[2c] (c+1 st), normaliserade så att de summerar till 0,5.
inline std::vector<float> designHalfbandEvenTaps(int c, double beta) {
    auto i0 = [](double x) { // modifierad Bessel I0 (serieutveckling)
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 50; ++k) { term *= (x / (2.0 * k)) * (x / (2.0 * k)); sum += term; }
        return sum;
    };
    const double pi = 3.14159265358979323846;
    std::vector<double> h(static_cast<size_t>(c + 1));
    double sum = 0.0;
    for (int i = 0; i <= c; ++i) {
        const int j = 2 * i;                  // index i hela filtret
        const double t = (j - c) / 2.0;       // udda halvtal => sinc != 0
        const double sinc = std::sin(pi * t) / (pi * t);
        const double r = (j - c) / static_cast<double>(c);
        const double w = i0(beta * std::sqrt(std::fmax(0.0, 1.0 - r * r))) / i0(beta);
        h[static_cast<size_t>(i)] = 0.5 * sinc * w;
        sum += h[static_cast<size_t>(i)];
    }
    std::vector<float> out(h.size());
    for (size_t i = 0; i < h.size(); ++i) out[i] = static_cast<float>(h[i] * 0.5 / sum);
    return out;
}

// Ett 2x-steg (upp och ner) för en kanal.
class HalfbandStage {
public:
    void prepare(int c, double beta) {
        taps_ = designHalfbandEvenTaps(c, beta);
        K_ = c + 1;
        d_ = (c - 1) / 2;
        up_.assign(static_cast<size_t>(2 * K_), 0.0f);
        dnA_.assign(static_cast<size_t>(2 * K_), 0.0f);
        dnB_.assign(static_cast<size_t>(2 * K_), 0.0f);
        reset();
    }

    void reset() {
        std::fill(up_.begin(), up_.end(), 0.0f);
        std::fill(dnA_.begin(), dnA_.end(), 0.0f);
        std::fill(dnB_.begin(), dnB_.end(), 0.0f);
        upPos_ = dnPos_ = 0;
    }

    // n insamples -> 2n utsamples.
    void up(const float* x, int n, float* y) noexcept {
        const float* h = taps_.data();
        float* b = up_.data();
        for (int i = 0; i < n; ++i) {
            upPos_ = (upPos_ == 0 ? K_ : upPos_) - 1;
            b[upPos_] = b[upPos_ + K_] = x[i];       // b[upPos_ + k] = x[i - k]
            const float* hist = b + upPos_;
            float acc = 0.0f;
            for (int k = 0; k < K_; ++k) acc += h[k] * hist[k];
            y[2 * i]     = 2.0f * acc;
            y[2 * i + 1] = hist[d_];
        }
    }

    // 2n insamples -> n utsamples.
    void down(const float* x, int n, float* y) noexcept {
        const float* h = taps_.data();
        float* a = dnA_.data();
        float* b = dnB_.data();
        for (int i = 0; i < n; ++i) {
            dnPos_ = (dnPos_ == 0 ? K_ : dnPos_) - 1;
            a[dnPos_] = a[dnPos_ + K_] = x[2 * i];
            b[dnPos_] = b[dnPos_ + K_] = x[2 * i + 1];
            const float* hb = b + dnPos_;
            float acc = 0.0f;
            for (int k = 0; k < K_; ++k) acc += h[k] * hb[k];
            y[i] = acc + 0.5f * a[dnPos_ + d_];
        }
    }

private:
    std::vector<float> taps_, up_, dnA_, dnB_;
    int K_ = 1, d_ = 0, upPos_ = 0, dnPos_ = 0;
};

} // namespace detail

class Oversampler4x {
public:
    // Steg 1 (1x<->2x) är kritiskt och behöver brant filter; steg 2 kan vara kortare.
    static constexpr int kC1 = 23, kC2 = 11;
    static constexpr double kBeta = 7.9; // ca 80 dB spärrbandsdämpning

    void prepare(int maxBlockSize, int numChannels) {
        numCh_ = numChannels;
        maxBlock_ = maxBlockSize;
        s1_.assign(static_cast<size_t>(numChannels), {});
        s2_.assign(static_cast<size_t>(numChannels), {});
        for (auto& s : s1_) s.prepare(kC1, kBeta);
        for (auto& s : s2_) s.prepare(kC2, kBeta);
        mid_.assign(static_cast<size_t>(maxBlockSize * 2), 0.0f);
    }

    void reset() {
        for (auto& s : s1_) s.reset();
        for (auto& s : s2_) s.reset();
    }

    // n (<= maxBlockSize) samples -> 4n samples i out4x.
    void up(int ch, const float* in, int n, float* out4x) noexcept {
        s1_[static_cast<size_t>(ch)].up(in, n, mid_.data());
        s2_[static_cast<size_t>(ch)].up(mid_.data(), 2 * n, out4x);
    }

    // 4n samples -> n samples.
    void down(int ch, const float* in4x, int n, float* out) noexcept {
        s2_[static_cast<size_t>(ch)].down(in4x, 2 * n, mid_.data());
        s1_[static_cast<size_t>(ch)].down(mid_.data(), n, out);
    }

    static constexpr int latencySamples() {
        // c1 - 0.5 + (2*c2 - 1)/4, avrundat.
        return static_cast<int>((4.0 * kC1 - 2.0 + 2.0 * kC2 - 1.0) / 4.0 + 0.5);
    }

private:
    std::vector<detail::HalfbandStage> s1_, s2_;
    std::vector<float> mid_;
    int numCh_ = 0, maxBlock_ = 0;
};

} // namespace gfx
