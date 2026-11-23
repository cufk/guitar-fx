#pragma once
// Dubblare och oktav: ett tjockare, bredare ljud – och en tolvsträngad känsla.
//
// Dubbling: två röster med kort fördröjning (≈ 0,7× och 1× tiden) som svävar långsamt i
// tonhöjd och tid, var sin åt vänster och höger. Låter som två tagningar av samma spel.
// Oktav: tonhöjden höjs en oktav med två läsare som springer dubbelt så fort genom en
// fördröjningslinje och tonas växelvis (≈30 ms cykel). När en läsare börjar om väljs
// startpunkten med korrelation så att vågformen ligger i fas med den andra läsaren –
// annars släcker de ut varandra och oktaven "fladdrar". Fördröjningen (10–40 ms) gör
// inget: oktavsträngarna på en tolvsträngad kommer också lite efter. Oktaven lågpassas
// så att den blir "glittrig" men inte vass.
//
// Dubbling 0 och oktav 0 = exakt genomgång.

#include "../Effect.h"
#include "../core/DelayLine.h"
#include "../core/Parameter.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace gfx {

class Doubler final : public Effect {
public:
    Parameter doubling { 0.5f, 0.0f, 1.0f };
    Parameter octave   { 0.0f, 0.0f, 1.0f };
    Parameter timeMs   { 18.0f, 8.0f, 35.0f };
    Parameter width    { 0.8f, 0.0f, 1.0f };

    std::string_view name() const override { return "Doubler"; }

    void prepare(const ProcessSpec& spec) override {
        sr_ = spec.sampleRate;
        numCh_ = std::min(spec.numChannels, kMaxChannels);
        line_.prepare(static_cast<int>(sr_ * 0.08) + 8);
        lfoA_.setSampleRate(sr_);
        lfoB_.setSampleRate(sr_);
        dbl_.prepare(sr_, 0.03);
        oct_.prepare(sr_, 0.03);
        time_.prepare(sr_, 0.1);
        width_.prepare(sr_, 0.05);
        windowSmp_ = static_cast<float>(0.030 * sr_);
        lpCoef_ = static_cast<float>(1.0 - std::exp(-2.0 * 3.14159265358979 * 5000.0 / sr_));
        reset();
    }

    void reset() override {
        line_.reset();
        lfoA_.reset(0.0);
        lfoB_.reset(0.37);
        taps_[0] = { kMinDelay + windowSmp_, kMinDelay + windowSmp_ };
        taps_[1] = { kMinDelay + 0.5f * windowSmp_, kMinDelay + windowSmp_ };
        lp_ = 0.0f;
        dbl_.setCurrentAndTarget(doubling.get());
        oct_.setCurrentAndTarget(octave.get());
        time_.setCurrentAndTarget(timeMs.get());
        width_.setCurrentAndTarget(width.get());
    }

    void process(AudioBufferView buf) override {
        dbl_.setTarget(doubling.get());
        oct_.setTarget(octave.get());
        time_.setTarget(timeMs.get());
        width_.setTarget(width.get());
        const int nCh = std::min(buf.numChannels, numCh_);
        if (nCh <= 0) return;
        if (!dbl_.isSmoothing() && !oct_.isSmoothing() && dbl_.target() <= 0.0f && oct_.target() <= 0.0f) {
            // Håll linjen aktuell så att ett påslag inte spelar upp gammalt ljud.
            for (int i = 0; i < buf.numSamples; ++i) line_.push(mono(buf, nCh, i));
            return;
        }
        const float ms = static_cast<float>(sr_ * 1e-3);
        for (int i = 0; i < buf.numSamples; ++i) {
            const float x = mono(buf, nCh, i);
            line_.push(x);
            const float d = dbl_.next(), o = oct_.next(), t = time_.next() * ms, w = width_.next();
            // Dubbling: två svävande röster.
            const float da = 0.7f * t + 1.2f * ms * Lfo::sine(lfoA_.advance(0.23f));
            const float db = t + 1.5f * ms * Lfo::sine(lfoB_.advance(0.31f));
            const float va = 0.7f * line_.readHermite(std::max(2.0f, da));
            const float vb = 0.7f * line_.readHermite(std::max(2.0f, db));
            // Oktav upp: varje läsare närmar sig nuet med 1 sample per sample (= dubbel fart).
            float shifted = 0.0f;
            if (o > 0.0f) {
                float g[2], y[2];
                for (int k = 0; k < 2; ++k) {
                    auto& tp = taps_[size_t(k)];
                    tp.d -= 1.0f;
                    if (tp.d < kMinDelay) {
                        tp.d = alignedStart(taps_[size_t(1 - k)].d);
                        tp.start = tp.d;
                    }
                    const float f = (tp.start - tp.d) / (tp.start - kMinDelay);
                    const float sn = std::sin(3.14159265f * f);
                    g[k] = sn * sn;
                    y[k] = line_.readLinear(tp.d);
                }
                shifted = (g[0] * y[0] + g[1] * y[1]) / std::max(0.5f, g[0] + g[1]);
            }
            lp_ += lpCoef_ * (shifted - lp_);
            const float octOut = o * 0.8f * lp_;
            const float near = 0.5f + 0.5f * w, far = 0.5f - 0.5f * w;
            if (nCh == 1) {
                buf.channels[0][i] += d * 0.5f * (va + vb) + octOut;
            } else {
                buf.channels[0][i] += d * (near * va + far * vb) + octOut;
                buf.channels[1][i] += d * (near * vb + far * va) + octOut;
            }
        }
    }

private:
    struct Tap { float d, start; };
    static constexpr float kMinDelay = 2.0f;

    // Ny startfördröjning ≈ ett fönster bakåt, justerad (±6 ms) så att vågformen framåt
    // stämmer bäst med det den andra läsaren spelar just nu. Ingen allokering.
    float alignedStart(float otherDelay) const {
        const float base = kMinDelay + windowSmp_;
        const int search = static_cast<int>(0.006 * sr_), len = static_cast<int>(0.005 * sr_);
        const int od = static_cast<int>(otherDelay);
        if (od < len + 2) return base;
        float best = -1.0e30f;
        int bestOff = 0;
        for (int off = -search; off <= search; ++off) {
            const int nd = static_cast<int>(base) + off;
            float c = 0.0f;
            for (int j = 0; j < len; j += 2) c += line_.readLinear(static_cast<float>(nd - j)) * line_.readLinear(static_cast<float>(od - j));
            if (c > best) { best = c; bestOff = off; }
        }
        return base + static_cast<float>(bestOff) + (otherDelay - static_cast<float>(od));
    }

    static float mono(const AudioBufferView& buf, int nCh, int i) {
        float m = 0.0f;
        for (int ch = 0; ch < nCh; ++ch) m += buf.channels[ch][i];
        return m / static_cast<float>(nCh);
    }

    double sr_ = 48000.0;
    int numCh_ = 2;
    DelayLine line_;
    Lfo lfoA_, lfoB_;
    SmoothedValue dbl_, oct_, time_, width_;
    float windowSmp_ = 1440.0f, lp_ = 0.0f, lpCoef_ = 0.5f;
    std::array<Tap, 2> taps_ {};
};

} // namespace gfx
