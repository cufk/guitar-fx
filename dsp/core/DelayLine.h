#pragma once
// Fördröjningslinje med bråkdels-läsning (för chorus, flanger, reverb).
// prepare() allokerar; push/read är realtidssäkra.

#include <algorithm>
#include <cmath>
#include <vector>

namespace gfx {

class DelayLine {
public:
    // maxDelaySamples: största fördröjning som kommer att läsas.
    void prepare(int maxDelaySamples) {
        int n = 4;
        while (n < maxDelaySamples + 4) n <<= 1;
        buf_.assign(static_cast<size_t>(n), 0.0f);
        mask_ = n - 1;
        maxDelay_ = static_cast<float>(n - 3);
        write_ = 0;
    }

    void reset() noexcept { std::fill(buf_.begin(), buf_.end(), 0.0f); write_ = 0; }

    void push(float x) noexcept {
        write_ = (write_ + 1) & mask_;
        buf_[static_cast<size_t>(write_)] = x;
    }

    // Fördröjning i samples relativt senast pushade sample (0 = senaste).
    float readLinear(float delay) const noexcept {
        delay = std::clamp(delay, 0.0f, maxDelay_);
        const int i = static_cast<int>(delay);
        const float f = delay - static_cast<float>(i);
        const float a = at(i), b = at(i + 1);
        return a + f * (b - a);
    }

    // 4-punkts Hermite: mindre diskantförlust och modulationsbrus än linjär.
    float readHermite(float delay) const noexcept {
        delay = std::clamp(delay, 1.0f, maxDelay_ - 1.0f);
        const int i = static_cast<int>(delay);
        const float f = delay - static_cast<float>(i);
        const float xm1 = at(i - 1), x0 = at(i), x1 = at(i + 1), x2 = at(i + 2);
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

private:
    float at(int delay) const noexcept { return buf_[static_cast<size_t>((write_ - delay) & mask_)]; }

    std::vector<float> buf_ { 0.0f, 0.0f, 0.0f, 0.0f };
    int mask_ = 3, write_ = 0;
    float maxDelay_ = 1.0f;
};

// Enkel sinus-LFO (fas 0..1).
class Lfo {
public:
    void setSampleRate(double sr) noexcept { invSr_ = 1.0 / sr; }
    void reset(double phase = 0.0) noexcept { phase_ = phase; }
    // Steg framåt med frekvens hz; returnerar fasen före steget.
    double advance(float hz) noexcept {
        const double p = phase_;
        phase_ += hz * invSr_;
        phase_ -= std::floor(phase_);
        return p;
    }
    static float sine(double phase) noexcept { return static_cast<float>(std::sin(6.283185307179586 * phase)); }

private:
    double phase_ = 0.0, invSr_ = 1.0 / 48000.0;
};

} // namespace gfx
