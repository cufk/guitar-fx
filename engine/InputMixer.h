#pragma once
// Ingångssteget: väljer eller blandar två ingångar till en mono-signal.
//
// Typiskt för akustisk gitarr på Scarlett 2i2:
//   ingång 1 = mikrofon mot gitarren (XLR, INST av, ev. 48V)
//   ingång 2 = gitarrens inbyggda mikrofon (teleplugg, INST på)
//
//   in1 -> fördröjning -> polaritet --+
//                                      +--> (balans) --> mono ut
//   in2 -> fördröjning -> polaritet --+
//
// Tidsjustering: mikrofonen hör gitarren lite senare än den inbyggda
// (ca 0,3 ms per 10 cm). Utan justering släcker signalerna delvis ut
// varandra (kamfilter) och det låter tunt. alignMs > 0 fördröjer ingång 2,
// alignMs < 0 fördröjer ingång 1. Bråkdelar av samples stöds.
//
// Automatisk justering: requestCapture() spelar in en sekund av båda
// ingångarna på ljudtråden; analyseCapture() (GUI-tråden) korrelerar dem
// och föreslår fördröjning och polaritet.
//
// Alla växlingar (källa, balans, polaritet, fördröjning) glider mjukt.

#include "dsp/core/Parameter.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <optional>
#include <vector>

namespace gfx {

struct AlignResult {
    double lagSamples = 0.0;  // > 0: ingång 2 kommer senare än ingång 1
    bool   invert = false;    // ingång 2 har motsatt polaritet
    double correlation = 0.0; // 0..1, hur säker analysen är
};

// Ren funktion (testbar): hitta fördröjning och polaritet mellan a och b.
// Söker inom ±maxLag samples. Signalerna differentieras först så att
// basen (som dominerar men ger en bred, otydlig topp) inte styr.
inline AlignResult analyseAlignment(const float* a, const float* b, int n, int maxLag) {
    AlignResult r;
    if (n <= 2 * maxLag + 2) return r;
    std::vector<float> da(static_cast<size_t>(n - 1)), db(static_cast<size_t>(n - 1));
    for (int i = 1; i < n; ++i) { da[size_t(i - 1)] = a[i] - a[i - 1]; db[size_t(i - 1)] = b[i] - b[i - 1]; }
    const int m = n - 1;

    double ea = 0.0, eb = 0.0;
    for (int i = maxLag; i < m - maxLag; ++i) { ea += double(da[size_t(i)]) * da[size_t(i)]; eb += double(db[size_t(i)]) * db[size_t(i)]; }
    if (ea <= 1e-12 || eb <= 1e-12) return r;
    const double norm = std::sqrt(ea * eb);

    // corr(L) = sum a[i] * b[i + L]
    std::vector<double> corr(static_cast<size_t>(2 * maxLag + 1));
    for (int L = -maxLag; L <= maxLag; ++L) {
        double s = 0.0;
        for (int i = maxLag; i < m - maxLag; ++i) s += double(da[size_t(i)]) * db[size_t(i + L)];
        corr[size_t(L + maxLag)] = s / norm;
    }
    int best = 0;
    for (int k = 1; k < int(corr.size()); ++k)
        if (std::fabs(corr[size_t(k)]) > std::fabs(corr[size_t(best)])) best = k;

    // Parabolisk interpolation runt toppen för bråkdels-noggrannhet.
    double frac = 0.0;
    if (best > 0 && best < int(corr.size()) - 1) {
        const double y0 = std::fabs(corr[size_t(best - 1)]), y1 = std::fabs(corr[size_t(best)]), y2 = std::fabs(corr[size_t(best + 1)]);
        const double den = y0 - 2.0 * y1 + y2;
        if (std::fabs(den) > 1e-12) frac = std::clamp(0.5 * (y0 - y2) / den, -0.5, 0.5);
    }
    r.lagSamples = (best - maxLag) + frac;
    r.invert = corr[size_t(best)] < 0.0;
    r.correlation = std::fabs(corr[size_t(best)]);
    return r;
}

class InputMixer {
public:
    enum Mode { Input1 = 0, Input2 = 1, Blend = 2 };
    static constexpr double kMaxAlignMs = 5.0;
    static constexpr double kCaptureSeconds = 1.0;

    std::atomic<int>  mode { Input1 };
    Parameter balance { 0.5f, 0.0f, 1.0f };   // 0 = bara ingång 1, 1 = bara ingång 2
    Parameter alignMs { 0.0f, float(-kMaxAlignMs), float(kMaxAlignMs) };
    std::atomic<bool> invert1 { false }, invert2 { false };

    void prepare(double sampleRate) {
        sr_ = sampleRate;
        const int maxDelay = static_cast<int>(std::ceil(kMaxAlignMs * 1e-3 * sampleRate)) + 4;
        size_ = 1;
        while (size_ < maxDelay) size_ <<= 1;
        line1_.assign(static_cast<size_t>(size_), 0.0f);
        line2_.assign(static_cast<size_t>(size_), 0.0f);
        cap1_.assign(static_cast<size_t>(kCaptureSeconds * sampleRate), 0.0f);
        cap2_.assign(cap1_.size(), 0.0f);
        g1_.prepare(sampleRate, 0.03);
        g2_.prepare(sampleRate, 0.03);
        delay_.prepare(sampleRate, 0.08);
        reset();
    }

    void reset() {
        std::fill(line1_.begin(), line1_.end(), 0.0f);
        std::fill(line2_.begin(), line2_.end(), 0.0f);
        write_ = 0;
        float t1, t2;
        targetGains(t1, t2);
        g1_.setCurrentAndTarget(t1);
        g2_.setCurrentAndTarget(t2);
        delay_.setCurrentAndTarget(targetDelaySamples());
    }

    // Ljudtråden. in2 får vara nullptr (mono-buss): då används ingång 1.
    // out får vara samma buffert som in1.
    void process(const float* in1, const float* in2, float* out, int n) noexcept {
        float t1, t2;
        targetGains(t1, t2);
        if (!in2) { t1 = invert1.load(std::memory_order_relaxed) ? -1.0f : 1.0f; t2 = 0.0f; }
        g1_.setTarget(t1);
        g2_.setTarget(t2);
        delay_.setTarget(targetDelaySamples());

        const bool capturing = capturing_.load(std::memory_order_acquire);
        const int mask = size_ - 1;
        for (int i = 0; i < n; ++i) {
            const float x1 = in1[i];
            const float x2 = in2 ? in2[i] : 0.0f;
            line1_[size_t(write_)] = x1;
            line2_[size_t(write_)] = x2;

            if (capturing) {
                cap1_[size_t(capPos_)] = x1;
                cap2_[size_t(capPos_)] = x2;
                if (++capPos_ >= int(cap1_.size())) {
                    capturing_.store(false, std::memory_order_relaxed);
                    captureReady_.store(true, std::memory_order_release);
                }
            }

            const float d = delay_.next();          // > 0: fördröj ingång 2
            const float y1 = read(line1_, d < 0.0f ? -d : 0.0f, mask);
            const float y2 = read(line2_, d > 0.0f ?  d : 0.0f, mask);
            out[i] = g1_.next() * y1 + g2_.next() * y2;
            write_ = (write_ + 1) & mask;
        }
    }

    // --- Automatisk justering (GUI-tråden) ---
    void requestCapture() {
        if (capturing_.load(std::memory_order_acquire)) return;
        captureReady_.store(false, std::memory_order_relaxed);
        capPos_ = 0;
        capturing_.store(true, std::memory_order_release);
    }
    bool isCapturing() const { return capturing_.load(std::memory_order_acquire); }

    // Returnerar ett resultat en gång när inspelningen är klar, annars inget.
    // Kräver att båda ingångarna har signal (spela medan den lyssnar).
    std::optional<AlignResult> analyseCapture() {
        if (!captureReady_.exchange(false, std::memory_order_acq_rel)) return std::nullopt;
        const int maxLag = static_cast<int>(kMaxAlignMs * 1e-3 * sr_);
        return analyseAlignment(cap1_.data(), cap2_.data(), int(cap1_.size()), maxLag);
    }

    // Tillämpa ett analysresultat: fördröj den tidigare ingången, vänd ingång 2 vid behov.
    void apply(const AlignResult& r) {
        alignMs.set(static_cast<float>(-r.lagSamples / sr_ * 1000.0));
        invert1.store(false);
        invert2.store(r.invert);
    }

    double sampleRate() const { return sr_; }

private:
    void targetGains(float& t1, float& t2) const noexcept {
        const float p1 = invert1.load(std::memory_order_relaxed) ? -1.0f : 1.0f;
        const float p2 = invert2.load(std::memory_order_relaxed) ? -1.0f : 1.0f;
        switch (mode.load(std::memory_order_relaxed)) {
            case Input2: t1 = 0.0f; t2 = p2; break;
            case Blend: {
                // Konstant effekt: mitten ger -3 dB per källa.
                const float b = balance.get() * 1.5707963f;
                t1 = p1 * std::cos(b);
                t2 = p2 * std::sin(b);
                break;
            }
            default: t1 = p1; t2 = 0.0f; break;
        }
    }

    float targetDelaySamples() const noexcept {
        return static_cast<float>(alignMs.get() * 1e-3 * sr_);
    }

    float read(const std::vector<float>& line, float delay, int mask) const noexcept {
        const int whole = static_cast<int>(delay);
        const float frac = delay - static_cast<float>(whole);
        const float a = line[size_t((write_ - whole) & mask)];
        const float b = line[size_t((write_ - whole - 1) & mask)];
        return a + frac * (b - a);
    }

    double sr_ = 48000.0;
    int size_ = 1, write_ = 0;
    std::vector<float> line1_, line2_, cap1_, cap2_;
    int capPos_ = 0;
    std::atomic<bool> capturing_ { false }, captureReady_ { false };
    SmoothedValue g1_, g2_, delay_;
};

} // namespace gfx
