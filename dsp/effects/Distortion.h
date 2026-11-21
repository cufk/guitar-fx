#pragma once
// Distortion/overdrive med 4x oversampling.
//
//   in -> lågskärning (tightar basen före klippning) -> drive
//      -> [4x upp -> waveshaper -> 4x ner] -> DC-spärr -> ton (lågpass) -> nivå
//
// Lägen:
//   0 Overdrive  – mjuk, symmetrisk (tanh-liknande), C1-kontinuerlig
//   1 Distortion – hårdare knä (kubisk klippare), mer kompression
//   2 Fuzz       – asymmetrisk, jämna övertoner (rör/germanium-karaktär)
//
// Alla waveshapers är begränsade till [-1, 1] och kontinuerliga i första
// derivatan, vilket håller övertonsinnehållet (och därmed resterande
// aliasing) nere även vid extrem drive.

#include "../Effect.h"
#include "../core/Biquad.h"
#include "../core/DspUtils.h"
#include "../core/Oversampler.h"
#include "../core/Parameter.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace gfx {

class Distortion final : public Effect {
public:
    enum Mode { Overdrive = 0, Hard = 1, Fuzz = 2 };

    Parameter driveDb { 18.0f, 0.0f, 48.0f };
    Parameter tone    { 0.5f, 0.0f, 1.0f };     // 0 = mörk (900 Hz), 1 = ljus (12 kHz)
    Parameter levelDb { -6.0f, -36.0f, 6.0f };
    Parameter mode    { 0.0f, 0.0f, 2.0f };
    Parameter tightHz { 120.0f, 20.0f, 400.0f }; // lågskärning före klippning

    std::string_view name() const override { return "Distortion"; }
    int latencySamples() const override { return Oversampler4x::latencySamples(); }

    void prepare(const ProcessSpec& spec) override {
        sr_ = spec.sampleRate;
        numCh_ = std::min(spec.numChannels, kMaxChannels);
        os_.prepare(kSubBlock, numCh_);
        up_.assign(static_cast<size_t>(kSubBlock * 4), 0.0f);
        drive_.prepare(sr_, 0.03);
        level_.prepare(sr_, 0.03);
        tone_.prepare(sr_, 0.03);
        dc_.prepare(sr_);
        reset();
    }

    void reset() override {
        drive_.setCurrentAndTarget(dbToGain(driveDb.get()));
        level_.setCurrentAndTarget(dbToGain(levelDb.get()));
        tone_.setCurrentAndTarget(tone.get());
        tight_.reset();
        lp_.reset();
        dc_.reset();
        os_.reset();
        updateFilters(true);
    }

    void process(AudioBufferView buf) override {
        drive_.setTarget(dbToGain(driveDb.get()));
        level_.setTarget(dbToGain(levelDb.get()));
        tone_.setTarget(tone.get());
        const int m = std::clamp(static_cast<int>(std::lround(mode.get())), 0, 2);
        const int nCh = std::min(buf.numChannels, numCh_);

        for (int start = 0; start < buf.numSamples; start += kSubBlock) {
            const int n = std::min(kSubBlock, buf.numSamples - start);
            updateFilters(false);

            for (int i = 0; i < n; ++i) { driveGain_[i] = drive_.next(); levelGain_[i] = level_.next(); }

            for (int ch = 0; ch < nCh; ++ch) {
                float* x = buf.channels[ch] + start;
                tight_.process(ch, x, n);
                for (int i = 0; i < n; ++i) x[i] *= driveGain_[i];

                float* u = up_.data();
                os_.up(ch, x, n, u);
                shape(m, u, 4 * n);
                os_.down(ch, u, n, x);

                dc_.process(ch, x, n);
                lp_.process(ch, x, n);
                for (int i = 0; i < n; ++i) x[i] *= levelGain_[i];
            }
        }
    }

    // Exponerad för tester.
    static float shapeSample(int m, float x) noexcept {
        switch (m) {
            case Hard: return cubicClip(x * 1.5f);
            case Fuzz: return (softClip(1.3f * x + kBias) - softClip(kBias)) / (1.0f + softClip(kBias));
            default:   return softClip(x);
        }
    }

private:
    static constexpr int kSubBlock = 32;
    static constexpr float kBias = 0.4f;

    // Rationell tanh-approximation, exakt ±1 och derivata 0 vid |x| = 3.
    static float softClip(float x) noexcept {
        if (!(x > -3.0f)) return -1.0f;          // fångar även NaN -> -1 (ändligt)
        if (x > 3.0f) return 1.0f;
        const float x2 = x * x;
        return std::clamp(x * (27.0f + x2) / (27.0f + 9.0f * x2), -1.0f, 1.0f);
    }
    static float cubicClip(float x) noexcept {
        if (!(x > -1.0f)) return -1.0f;
        if (x > 1.0f) return 1.0f;
        return x * (1.5f - 0.5f * x * x);
    }

    static void shape(int m, float* d, int n) noexcept {
        for (int i = 0; i < n; ++i) d[i] = shapeSample(m, d[i]);
    }

    void updateFilters(bool force) noexcept {
        const float th = tightHz.get();
        if (force || th < lastTight_ || th > lastTight_) { tight_.setCoeffs(BiquadCoeffs::highpass(sr_, th, 0.6)); lastTight_ = th; }
        if (force || tone_.isSmoothing() || tone_.target() < lastTone_ || tone_.target() > lastTone_) {
            const float t = tone_.current();
            const double hz = 900.0 * std::pow(12000.0 / 900.0, static_cast<double>(t));
            lp_.setCoeffs(BiquadCoeffs::lowpass(sr_, hz, 0.6));
            lastTone_ = tone_.target();
        }
        // Låt tonen glida en delblocks-längd framåt.
        for (int i = 0; i < kSubBlock && tone_.isSmoothing(); ++i) tone_.next();
    }

    double sr_ = 48000.0;
    int numCh_ = 2;
    Oversampler4x os_;
    std::vector<float> up_;
    float driveGain_[kSubBlock] {}, levelGain_[kSubBlock] {};
    SmoothedValue drive_, level_, tone_;
    Biquad tight_, lp_;
    DcBlocker dc_;
    float lastTight_ = -1.0f, lastTone_ = -1.0f;
};

} // namespace gfx
