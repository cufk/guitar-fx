#pragma once
// Flanger: kort fördröjning (0,3–5 ms) som sveper, med återkoppling.
//
// Återkopplingen går genom softClip(), så slingan kan aldrig växa obegränsat
// även med max återkoppling och hög insignal. Negativ återkoppling ger en
// ihåligare klang, positiv en mer "metallisk".
// Höger kanal sveper 90° förskjutet (stereo). Mix 0 = exakt genomgång.

#include "../Effect.h"
#include "../core/DelayLine.h"
#include "../core/DspUtils.h"
#include "../core/Parameter.h"
#include <algorithm>
#include <array>

namespace gfx {

class Flanger final : public Effect {
public:
    Parameter rateHz   { 0.25f, 0.05f, 5.0f };
    Parameter depth    { 0.7f, 0.0f, 1.0f };
    Parameter feedback { 0.5f, -0.95f, 0.95f };
    Parameter mix      { 0.5f, 0.0f, 1.0f };

    std::string_view name() const override { return "Flanger"; }

    void prepare(const ProcessSpec& spec) override {
        sr_ = spec.sampleRate;
        numCh_ = std::min(spec.numChannels, kMaxChannels);
        const int maxDelay = static_cast<int>(kMaxMs * 1e-3 * sr_) + 4;
        for (auto& l : lines_) l.prepare(maxDelay);
        lfo_.setSampleRate(sr_);
        rate_.prepare(sr_, 0.05);
        depth_.prepare(sr_, 0.05);
        fb_.prepare(sr_, 0.03);
        mix_.prepare(sr_, 0.03);
        reset();
    }

    void reset() override {
        for (auto& l : lines_) l.reset();
        lfo_.reset();
        rate_.setCurrentAndTarget(rateHz.get());
        depth_.setCurrentAndTarget(depth.get());
        fb_.setCurrentAndTarget(feedback.get());
        mix_.setCurrentAndTarget(mix.get());
    }

    void process(AudioBufferView buf) override {
        rate_.setTarget(rateHz.get());
        depth_.setTarget(depth.get());
        fb_.setTarget(feedback.get());
        mix_.setTarget(mix.get());
        const int nCh = std::min(buf.numChannels, numCh_);
        const float msToSmp = static_cast<float>(sr_ * 1e-3);
        const float minD = static_cast<float>(kMinMs) * msToSmp;
        const float range = static_cast<float>(kMaxMs - kMinMs) * msToSmp;

        for (int i = 0; i < buf.numSamples; ++i) {
            const double phase = lfo_.advance(rate_.next());
            const float dep = depth_.next();
            const float fb = fb_.next();
            const float m = mix_.next();
            for (int ch = 0; ch < nCh; ++ch) {
                auto& line = lines_[static_cast<size_t>(ch)];
                const float x = buf.channels[ch][i];
                const double off = (ch & 1) ? 0.25 : 0.0;
                const float sweep = 0.5f * (1.0f + Lfo::sine(phase + off));   // 0..1
                const float d = minD + range * dep * sweep;
                const float wet = line.readHermite(d);
                line.push(x + fb * softClip(wet));
                buf.channels[ch][i] = x + m * (wet - x);
            }
        }
    }

private:
    static constexpr double kMinMs = 0.3, kMaxMs = 5.0;
    double sr_ = 48000.0;
    int numCh_ = 2;
    std::array<DelayLine, kMaxChannels> lines_;
    Lfo lfo_;
    SmoothedValue rate_, depth_, fb_, mix_;
};

} // namespace gfx
