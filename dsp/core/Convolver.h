#pragma once
// Faltning utan latens (för kabinett-IR).
//
// Metod: IR:ns första B samples ("huvudet") faltas direkt i tidsdomänen,
// resten ("svansen") med uniformt partitionerad FFT-faltning (overlap-save,
// partitioner om B samples, FFT-storlek 2B). Svansens bidrag till ett
// utblock beror bara på indata som redan har kommit in, så ingen extra
// latens uppstår – oberoende av värdens buffertstorlek.
//
// ConvolverIR byggs utanför ljudtråden. Convolver (tillstånd per kanal)
// allokerar i prepare() och är sedan realtidssäker.

#include "FFT.h"
#include <algorithm>
#include <complex>
#include <vector>

namespace gfx {

struct ConvolverIR {
    static constexpr int kPartition = 64;      // B
    static constexpr int kFftSize = 2 * kPartition;

    int length = 0;                            // IR-längd i samples
    int numParts = 0;                          // inkl. huvudet
    std::vector<float> head;                   // B koefficienter, omvänd ordning för dot-produkt
    std::vector<std::complex<float>> tail;     // (numParts-1) * kFftSize spektra

    // Ej realtid. ir: samples vid motorns samplingsfrekvens.
    static ConvolverIR* build(const float* ir, int n, const FFT& fft) {
        auto* c = new ConvolverIR();
        constexpr int B = kPartition;
        c->length = std::max(n, 1);
        c->numParts = (c->length + B - 1) / B;
        c->head.assign(B, 0.0f);
        for (int i = 0; i < std::min(n, B); ++i) c->head[static_cast<size_t>(B - 1 - i)] = ir[i];

        const int tailParts = c->numParts - 1;
        c->tail.assign(static_cast<size_t>(tailParts * kFftSize), {});
        for (int k = 1; k < c->numParts; ++k) {
            auto* H = c->tail.data() + static_cast<size_t>((k - 1) * kFftSize);
            for (int i = 0; i < B; ++i) {
                const int src = k * B + i;
                H[i] = src < n ? ir[src] : 0.0f;  // nollutfyllt till 2B
            }
            fft.forward(H);
        }
        return c;
    }
};

class Convolver {
public:
    static constexpr int B = ConvolverIR::kPartition;
    static constexpr int N = ConvolverIR::kFftSize;
    using cfloat = std::complex<float>;

    void prepare(int maxParts) {
        maxParts_ = std::max(1, maxParts);
        fdl_.assign(static_cast<size_t>(maxParts_ * N), {});
        acc_.assign(N, {});
        reset();
    }

    void reset() noexcept {
        std::fill(fdl_.begin(), fdl_.end(), cfloat {});
        std::fill(std::begin(hist_), std::end(hist_), 0.0f);
        std::fill(std::begin(prev_), std::end(prev_), 0.0f);
        std::fill(std::begin(cur_), std::end(cur_), 0.0f);
        std::fill(std::begin(tailOut_), std::end(tailOut_), 0.0f);
        pos_ = 0; histPos_ = 0; fdlPos_ = 0;
    }

    // ir får vara nullptr (=> tystnad). fft måste ha storlek N.
    void process(const ConvolverIR* ir, const FFT& fft, float* d, int n) noexcept {
        if (!ir) { std::fill(d, d + n, 0.0f); return; }
        const float* h = ir->head.data();
        for (int i = 0; i < n; ++i) {
            const float x = d[i];
            cur_[pos_] = x;
            // Historik i dubbelbuffert: hist_[histPos_ .. histPos_+B) = senaste B, äldst först.
            hist_[histPos_] = hist_[histPos_ + B] = x;
            histPos_ = (histPos_ + 1) % B;
            const float* hp = hist_ + histPos_;
            float y = 0.0f;
            for (int k = 0; k < B; ++k) y += h[k] * hp[k];
            d[i] = y + tailOut_[pos_];

            if (++pos_ == B) blockBoundary(ir, fft);
        }
    }

private:
    void blockBoundary(const ConvolverIR* ir, const FFT& fft) noexcept {
        pos_ = 0;
        // Ram = [föregående block, nuvarande block] -> spektrum i FDL.
        fdlPos_ = (fdlPos_ + 1) % maxParts_;
        cfloat* X = fdl_.data() + static_cast<size_t>(fdlPos_ * N);
        for (int i = 0; i < B; ++i) { X[i] = prev_[i]; X[B + i] = cur_[i]; }
        fft.forward(X);
        std::copy(std::begin(cur_), std::end(cur_), prev_);

        const int parts = std::min(ir->numParts, maxParts_);
        if (parts <= 1) { std::fill(std::begin(tailOut_), std::end(tailOut_), 0.0f); return; }

        // acc = sum_{k>=1} X_{m-k} * H_k, där X_{m-1} är det just beräknade.
        std::fill(acc_.begin(), acc_.end(), cfloat {});
        float* a = reinterpret_cast<float*>(acc_.data());
        for (int k = 1; k < parts; ++k) {
            const int idx = (fdlPos_ - (k - 1) + maxParts_) % maxParts_;
            const float* x = reinterpret_cast<const float*>(fdl_.data() + static_cast<size_t>(idx * N));
            const float* hk = reinterpret_cast<const float*>(ir->tail.data() + static_cast<size_t>((k - 1) * N));
            for (int j = 0; j < 2 * N; j += 2) {
                a[j]     += x[j] * hk[j]     - x[j + 1] * hk[j + 1];
                a[j + 1] += x[j] * hk[j + 1] + x[j + 1] * hk[j];
            }
        }
        fft.inverse(acc_.data());
        for (int i = 0; i < B; ++i) tailOut_[i] = acc_[static_cast<size_t>(B + i)].real();
    }

    int maxParts_ = 1, pos_ = 0, histPos_ = 0, fdlPos_ = 0;
    std::vector<cfloat> fdl_, acc_;
    float hist_[2 * B] {}, prev_[B] {}, cur_[B] {}, tailOut_[B] {};
};

} // namespace gfx
