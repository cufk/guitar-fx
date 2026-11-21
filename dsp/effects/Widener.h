#pragma once
// Widener: gör gitarren bredare i stereo – helt monokompatibel.
//
// Mitt/sida: sidosignalen får ett tillskott av fördröjd, lågskuren mittsignal:
//     S' = S + bredd * HP(fördröj(M))      L = M + S'   R = M - S'
// Summeras vänster och höger blir resultatet exakt M – inga kamfilter i ett
// mono-PA. Lågskärningen håller basen i mitten (tight och fokuserad).
// Kräver två utgångar för att höras; med "Mono (summa)" påverkar den inte ljudet.

#include "../Effect.h"
#include "../core/Biquad.h"
#include "../core/DelayLine.h"
#include "../core/Parameter.h"
#include <algorithm>

namespace gfx {

class Widener final : public Effect {
public:
    Parameter width    { 0.5f, 0.0f, 1.0f };
    Parameter timeMs   { 12.0f, 3.0f, 30.0f };
    Parameter lowCutHz { 250.0f, 50.0f, 800.0f };

    std::string_view name() const override { return "Widener"; }

    void prepare(const ProcessSpec& spec) override {
        sr_ = spec.sampleRate;
        line_.prepare(static_cast<int>(30.0 * 1e-3 * sr_) + 8);
        width_.prepare(sr_, 0.05);
        time_.prepare(sr_, 0.1);
        reset();
    }

    void reset() override {
        line_.reset();
        hp_.reset();
        width_.setCurrentAndTarget(width.get());
        time_.setCurrentAndTarget(timeMs.get());
        lastLowCut_ = -1.0f;
        updateFilter();
    }

    void process(AudioBufferView buf) override {
        if (buf.numChannels < 2) return;
        width_.setTarget(width.get());
        time_.setTarget(timeMs.get());
        updateFilter();
        const float msToSmp = static_cast<float>(sr_ * 1e-3);
        float* L = buf.channels[0];
        float* R = buf.channels[1];
        for (int i = 0; i < buf.numSamples; ++i) {
            const float m = 0.5f * (L[i] + R[i]);
            const float s = 0.5f * (L[i] - R[i]);
            line_.push(m);
            const float side = hp_.processSample(0, line_.readLinear(time_.next() * msToSmp));
            const float s2 = s + 0.7f * width_.next() * side;
            L[i] = m + s2;
            R[i] = m - s2;
        }
        // Fler än två kanaler: övriga lämnas orörda.
    }

private:
    void updateFilter() noexcept {
        const float lc = lowCutHz.get();
        if (lc < lastLowCut_ || lc > lastLowCut_) { hp_.setCoeffs(BiquadCoeffs::highpass(sr_, lc)); lastLowCut_ = lc; }
    }

    double sr_ = 48000.0;
    DelayLine line_;
    Biquad hp_;
    SmoothedValue width_, time_;
    float lastLowCut_ = -1.0f;
};

} // namespace gfx
