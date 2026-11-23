#pragma once
// Delay i takt: tempo (BPM) och indelning (1/4, 1/8 punkterad, 1/8, 1/16).
//
// - Tempot kan tappas (engine/TapTempo.h), sparas per preset eller hämtas från låtanalysen.
// - Ekona dämpas i återkopplingen (lågpass), så att de blir mörkare för varje varv –
//   låter naturligt och håller akustisk gitarr klar.
// - Tidsändringar glider (≈80 ms), så att tap tempo och preset-byten inte klickar.
// - Torr signal passerar oförändrad; mix lägger till ekona (mix 0 = exakt genomgång).
// - Svansen klingar ut när delayn stängs av (Effect::hasTail).

#include "../Effect.h"
#include "../core/DelayLine.h"
#include "../core/Parameter.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace gfx {

class Delay final : public Effect {
public:
    Parameter tempoBpm { 100.0f, 40.0f, 240.0f };
    Parameter division { 1.0f, 0.0f, 3.0f };      // 0 = 1/4, 1 = 1/8 punkterad, 2 = 1/8, 3 = 1/16
    Parameter feedback { 0.3f, 0.0f, 0.85f };
    Parameter dampHz   { 5000.0f, 1500.0f, 12000.0f };
    Parameter mix      { 0.25f, 0.0f, 1.0f };

    static float divisionFactor(int d) {
        static constexpr float f[4] = { 1.0f, 0.75f, 0.5f, 0.25f };
        return f[std::clamp(d, 0, 3)];
    }
    static float delayMs(float bpm, int div) { return 60000.0f / std::clamp(bpm, 40.0f, 240.0f) * divisionFactor(div); }

    std::string_view name() const override { return "Delay"; }

    bool hasTail() const override { return true; }
    void setInputActive(bool on) override { active_.setTarget(on ? 1.0f : 0.0f); }
    bool tailFinished() const override {
        return !active_.isSmoothing() && active_.target() <= 0.0f && quietSamples_ > static_cast<int>(sr_ * 0.2);
    }

    void prepare(const ProcessSpec& spec) override {
        sr_ = spec.sampleRate;
        numCh_ = std::min(spec.numChannels, kMaxChannels);
        const int maxDelay = static_cast<int>(sr_ * (delayMs(40.0f, 0) * 1e-3 + 0.05)) + 8;
        for (auto& l : lines_) l.prepare(maxDelay);
        mix_.prepare(sr_, 0.03);
        fb_.prepare(sr_, 0.03);
        active_.prepare(sr_, 0.03);
        active_.setCurrentAndTarget(1.0f);
        glide_ = static_cast<float>(1.0 - std::exp(-1.0 / (0.08 * sr_)));
        reset();
    }

    void reset() override {
        for (auto& l : lines_) l.reset();
        lp_.fill(0.0f);
        delaySmp_ = targetSamples();
        mix_.setCurrentAndTarget(mix.get());
        fb_.setCurrentAndTarget(feedback.get());
        quietSamples_ = 0;
    }

    void process(AudioBufferView buf) override {
        mix_.setTarget(mix.get());
        fb_.setTarget(feedback.get());
        const float target = targetSamples();
        const float lpCoef = static_cast<float>(1.0 - std::exp(-2.0 * 3.14159265358979 * dampHz.get() / sr_));
        const int nCh = std::min(buf.numChannels, numCh_);
        float peak = 0.0f;
        for (int i = 0; i < buf.numSamples; ++i) {
            delaySmp_ += (target - delaySmp_) * glide_;
            const float m = mix_.next(), fb = fb_.next(), act = active_.next();
            for (int ch = 0; ch < nCh; ++ch) {
                auto& line = lines_[size_t(ch)];
                const float x = buf.channels[ch][i];
                // Läs före push: fördröjning delaySmp_ - 1 relativt senast pushade = delaySmp_ samples.
                const float wet = line.readHermite(delaySmp_ - 1.0f);
                float& lp = lp_[size_t(ch)];
                lp += lpCoef * (wet - lp);
                line.push(x * act + fb * lp);
                buf.channels[ch][i] = x + m * wet;
                peak = std::max(peak, std::fabs(wet));
            }
        }
        // Tyst = under −80 dBFS (ohörbart efter limitern).
        quietSamples_ = peak < 1.0e-4f ? quietSamples_ + buf.numSamples : 0;
    }

private:
    float targetSamples() const {
        const int div = static_cast<int>(std::lround(division.get()));
        return std::max(2.0f, delayMs(tempoBpm.get(), div) * 1e-3f * static_cast<float>(sr_));
    }

    double sr_ = 48000.0;
    int numCh_ = 2;
    std::array<DelayLine, kMaxChannels> lines_;
    std::array<float, kMaxChannels> lp_ {};
    float delaySmp_ = 1000.0f, glide_ = 0.001f;
    SmoothedValue mix_, fb_, active_;
    int quietSamples_ = 0;
};

} // namespace gfx
