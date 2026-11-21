#pragma once
// Enkel iterativ radix-2 komplex FFT utan beroenden.
// prepare() allokerar tabeller (ej realtid); forward/inverse är realtidssäkra.

#include <cmath>
#include <complex>
#include <vector>

namespace gfx {

class FFT {
public:
    using cfloat = std::complex<float>;

    void prepare(int size) {
        n_ = size;
        int bits = 0;
        while ((1 << bits) < n_) ++bits;
        rev_.assign(static_cast<size_t>(n_), 0);
        for (int i = 0; i < n_; ++i) {
            int r = 0;
            for (int b = 0; b < bits; ++b) if (i & (1 << b)) r |= 1 << (bits - 1 - b);
            rev_[static_cast<size_t>(i)] = r;
        }
        tw_.resize(static_cast<size_t>(n_ / 2));
        for (int k = 0; k < n_ / 2; ++k) {
            const double a = -2.0 * 3.14159265358979323846 * k / n_;
            tw_[static_cast<size_t>(k)] = cfloat(float(std::cos(a)), float(std::sin(a)));
        }
    }

    int size() const { return n_; }

    void forward(cfloat* d) const noexcept { transform(d, false); }

    // Omvänd transform, normaliserad med 1/N.
    void inverse(cfloat* d) const noexcept {
        transform(d, true);
        const float s = 1.0f / static_cast<float>(n_);
        for (int i = 0; i < n_; ++i) d[i] *= s;
    }

private:
    void transform(cfloat* d, bool inv) const noexcept {
        for (int i = 0; i < n_; ++i) {
            const int r = rev_[static_cast<size_t>(i)];
            if (r > i) std::swap(d[i], d[r]);
        }
        for (int len = 2; len <= n_; len <<= 1) {
            const int half = len >> 1, step = n_ / len;
            for (int i = 0; i < n_; i += len)
                for (int k = 0; k < half; ++k) {
                    const cfloat w = tw_[static_cast<size_t>(k * step)];
                    const float wr = w.real(), wi = inv ? -w.imag() : w.imag();
                    const cfloat a = d[i + k], x = d[i + k + half];
                    // Manuell multiplikation: undviker std::complex långsamma NaN-väg.
                    const cfloat b(x.real() * wr - x.imag() * wi, x.real() * wi + x.imag() * wr);
                    d[i + k] = a + b;
                    d[i + k + half] = a - b;
                }
        }
    }

    int n_ = 0;
    std::vector<int> rev_;
    std::vector<cfloat> tw_;
};

} // namespace gfx
