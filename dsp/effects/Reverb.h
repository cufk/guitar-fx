#pragma once
// Reverb: Feedback Delay Network (FDN) med 8 linjer.
//
//   mono in -> predelay -> 4 allpass-diffusorer -> 8 fördröjningslinjer
//                                                  med Householder-återkoppling,
//                                                  dämpning (lågpass) per linje
//   vänster ut = linje 0,2,4,6   höger ut = linje 1,3,5,7   -> lågskärning -> mix
//
// - Efterklangstid (RT60) ställs exakt via förstärkningen per linje.
// - Householder-matrisen är ortogonal och alla förstärkningar < 1, så
//   slingan är alltid stabil oavsett parametrar.
// - Två linjer moduleras långsamt för att undvika metallisk ringning.
// - Lågskärning på efterklangen (standard 150 Hz) håller akustisk gitarr
//   klar i mixen – basen i rummet gör den annars grötig.
// - Mix: torr signal ligger kvar på full nivå upp till mix 0,5.

#include "../Effect.h"
#include "../core/Biquad.h"
#include "../core/DelayLine.h"
#include "../core/DspUtils.h"
#include "../core/Parameter.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace gfx {

class Reverb final : public Effect {
public:
    Parameter decaySec  { 2.0f, 0.3f, 10.0f };
    Parameter size      { 0.6f, 0.0f, 1.0f };      // rumsstorlek (linjelängder)
    Parameter predelayMs{ 15.0f, 0.0f, 120.0f };
    Parameter dampHz    { 6000.0f, 1500.0f, 16000.0f };
    Parameter lowCutHz  { 150.0f, 20.0f, 600.0f };
    Parameter mix       { 0.25f, 0.0f, 1.0f };

    std::string_view name() const override { return "Reverb"; }

    // Svansen klingar ut när reverben stängs av (se Effect::hasTail).
    bool hasTail() const override { return true; }
    void setInputActive(bool on) override { active_.setTarget(on ? 1.0f : 0.0f); }
    bool tailFinished() const override {
        return !active_.isSmoothing() && active_.target() <= 0.0f && quietSamples_ > static_cast<int>(sr_ * 0.2);
    }

    void prepare(const ProcessSpec& spec) override {
        sr_ = spec.sampleRate;
        numCh_ = std::min(spec.numChannels, kMaxChannels);
        const float ms = static_cast<float>(sr_ * 1e-3);
        pre_.prepare(static_cast<int>(120.0f * ms) + 4);
        for (int k = 0; k < kDiff; ++k) diff_[size_t(k)].prepare(static_cast<int>(kDiffMs[k] * ms) + 4);
        for (int k = 0; k < kN; ++k)
            lines_[size_t(k)].prepare(static_cast<int>(kLineMs[k] * kMaxScale * ms + kModMs * ms) + 8);
        lfo_.setSampleRate(sr_);
        decay_.prepare(sr_, 0.1);
        size_.prepare(sr_, 0.3);
        pred_.prepare(sr_, 0.1);
        mix_.prepare(sr_, 0.03);
        active_.prepare(sr_, 0.03);
        active_.setCurrentAndTarget(1.0f);
        reset();
    }

    void reset() override {
        pre_.reset();
        for (auto& d : diff_) d.reset();
        for (auto& l : lines_) l.reset();
        damp_.fill(0.0f);
        lowCut_.reset();
        lfo_.reset();
        decay_.setCurrentAndTarget(decaySec.get());
        size_.setCurrentAndTarget(size.get());
        pred_.setCurrentAndTarget(predelayMs.get());
        mix_.setCurrentAndTarget(mix.get());
        quietSamples_ = 0;
        lastLowCut_ = -1.0f;
        lastDamp_ = -1.0f;
        updateFilters();
    }

    void process(AudioBufferView buf) override {
        decay_.setTarget(decaySec.get());
        size_.setTarget(size.get());
        pred_.setTarget(predelayMs.get());
        mix_.setTarget(mix.get());
        updateFilters();

        const int nCh = std::min(buf.numChannels, numCh_);
        if (nCh <= 0) return;
        const float ms = static_cast<float>(sr_ * 1e-3);
        const float invCh = 1.0f / static_cast<float>(nCh);

        for (int i = 0; i < buf.numSamples; ++i) {
            // Långsamma parametrar glider per sample; linjelängder/förstärkningar räknas om var 32:a sample.
            const float rt = decay_.next();
            const float scale = kMinScale + (kMaxScale - kMinScale) * size_.next();
            const float pd = pred_.next() * ms;
            const float m = mix_.next();
            const float act = active_.next();   // 1 = på, 0 = av (svansen klingar ut)
            if ((i & 31) == 0) updateGains(rt, scale, ms);

            // 1. Mono in -> predelay -> diffusorer.
            float x = 0.0f;
            for (int ch = 0; ch < nCh; ++ch) x += buf.channels[ch][i];
            x *= invCh * act;
            pre_.push(x);
            float s = pre_.readLinear(pd);
            for (int k = 0; k < kDiff; ++k) s = allpass(diff_[size_t(k)], s, kDiffMs[k] * ms, 0.6f);

            // 2. FDN.
            const double phase = lfo_.advance(0.37f);
            float v[kN];
            float sum = 0.0f;
            for (int k = 0; k < kN; ++k) {
                float d = lens_[size_t(k)];
                if (k == 0) d += kModMs * ms * Lfo::sine(phase);
                if (k == 5) d += kModMs * ms * Lfo::sine(phase + 0.33);
                const float y = lines_[size_t(k)].readLinear(d);
                out_[size_t(k)] = y;
                damp_[size_t(k)] += dampCoeff_ * (y - damp_[size_t(k)]);
                v[k] = gains_[size_t(k)] * damp_[size_t(k)];
                sum += v[k];
            }
            const float h = sum * (2.0f / kN);
            for (int k = 0; k < kN; ++k)
                lines_[size_t(k)].push(v[k] - h + kInSign[k] * 0.35f * s);

            // 3. Stereo ut.
            float wl = 0.0f, wr = 0.0f;
            for (int k = 0; k < kN; k += 2) { wl += kOutSign[k] * out_[size_t(k)]; wr += kOutSign[k + 1] * out_[size_t(k + 1)]; }
            wl = lowCut_.processSample(0, wl * 0.6f);
            wr = lowCut_.processSample(1, wr * 0.6f);

            // Medan reverben är av går torr signal oförändrad (dryG -> 1).
            const float dryG = 1.0f + act * (std::min(1.0f, 2.0f * (1.0f - m)) - 1.0f);
            const float wetG = std::min(1.0f, 2.0f * m);
            const float wetPeak = std::max(std::fabs(wl), std::fabs(wr));
            quietSamples_ = (wetPeak < 1.0e-5f) ? quietSamples_ + 1 : 0;
            for (int ch = 0; ch < nCh; ++ch) {
                const float wet = (ch & 1) ? wr : wl;
                buf.channels[ch][i] = dryG * buf.channels[ch][i] + wetG * wet;
            }
        }
    }

private:
    static constexpr int kN = 8, kDiff = 4;
    static constexpr float kLineMs[kN]  = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.1f, 73.3f };
    static constexpr float kDiffMs[kDiff] = { 4.77f, 3.59f, 12.73f, 9.30f };
    static constexpr float kInSign[kN]  = { 1, -1, 1, -1, -1, 1, -1, 1 };
    static constexpr float kOutSign[kN] = { 1, 1, -1, 1, 1, -1, -1, -1 };
    static constexpr float kMinScale = 0.5f, kMaxScale = 1.6f, kModMs = 0.35f;

    static float allpass(DelayLine& d, float x, float delaySmp, float g) noexcept {
        const float z = d.readLinear(delaySmp - 1.0f);
        const float w = x + g * z;
        d.push(w);
        return z - g * w;
    }

    void updateGains(float rt, float scale, float ms) noexcept {
        for (int k = 0; k < kN; ++k) {
            const float len = kLineMs[k] * scale * ms;
            lens_[size_t(k)] = len;
            // RT60: -60 dB efter rt sekunder => g = 10^(-3 * len / (rt * sr)).
            gains_[size_t(k)] = std::pow(10.0f, -3.0f * len / (rt * static_cast<float>(sr_)));
        }
    }

    void updateFilters() noexcept {
        const float lc = lowCutHz.get(), dh = dampHz.get();
        if (lc < lastLowCut_ || lc > lastLowCut_) { lowCut_.setCoeffs(BiquadCoeffs::highpass(sr_, lc)); lastLowCut_ = lc; }
        if (dh < lastDamp_ || dh > lastDamp_) {
            dampCoeff_ = static_cast<float>(1.0 - std::exp(-2.0 * 3.14159265358979 * dh / sr_));
            lastDamp_ = dh;
        }
    }

    double sr_ = 48000.0;
    int numCh_ = 2;
    DelayLine pre_;
    std::array<DelayLine, kDiff> diff_;
    std::array<DelayLine, kN> lines_;
    std::array<float, kN> lens_ {}, gains_ {}, damp_ {}, out_ {};
    float dampCoeff_ = 0.5f, lastLowCut_ = -1.0f, lastDamp_ = -1.0f;
    Biquad lowCut_;
    Lfo lfo_;
    SmoothedValue decay_, size_, pred_, mix_, active_;
    int quietSamples_ = 0;
};

} // namespace gfx
