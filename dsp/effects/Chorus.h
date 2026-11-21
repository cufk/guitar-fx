#pragma once
// Chorus: två modulerade fördröjda röster per kanal blandas med torr signal.
//
// Stereo: vänster och höger har LFO:er i olika fas, så även en mono-gitarr
// blir bred. På bara en utgång (t.ex. Line Out Left till mixern) låter den
// fortfarande som en vanlig chorus.
//
// Torr signal fördröjs inte – mix 0 är exakt genomgång, ingen latens.

#include "../Effect.h"
#include "../core/DelayLine.h"
#include "../core/Parameter.h"
#include <algorithm>
#include <array>

namespace gfx {

class Chorus final : public Effect {
public:
    Parameter rateHz { 0.8f, 0.05f, 5.0f };
    Parameter depth  { 0.5f, 0.0f, 1.0f };   // 0..1 -> 0..4 ms svep
    Parameter mix    { 0.4f, 0.0f, 1.0f };   // 0 = torr, 1 = bara röster (vibrato)

    std::string_view name() const override { return "Chorus"; }

    void prepare(const ProcessSpec& spec) override {
        sr_ = spec.sampleRate;
        numCh_ = std::min(spec.numChannels, kMaxChannels);
        const int maxDelay = static_cast<int>((kBaseMs + kMaxDepthMs + 1.0) * 1e-3 * sr_) + 4;
        for (auto& l : lines_) l.prepare(maxDelay);
        lfo_.setSampleRate(sr_);
        rate_.prepare(sr_, 0.05);
        depth_.prepare(sr_, 0.05);
        mix_.prepare(sr_, 0.03);
        reset();
    }

    void reset() override {
        for (auto& l : lines_) l.reset();
        lfo_.reset();
        rate_.setCurrentAndTarget(rateHz.get());
        depth_.setCurrentAndTarget(depth.get());
        mix_.setCurrentAndTarget(mix.get());
    }

    void process(AudioBufferView buf) override {
        rate_.setTarget(rateHz.get());
        depth_.setTarget(depth.get());
        mix_.setTarget(mix.get());
        const int nCh = std::min(buf.numChannels, numCh_);
        const float msToSmp = static_cast<float>(sr_ * 1e-3);
        const float base = static_cast<float>(kBaseMs) * msToSmp;

        for (int i = 0; i < buf.numSamples; ++i) {
            const double phase = lfo_.advance(rate_.next());
            const float dep = depth_.next() * static_cast<float>(kMaxDepthMs) * 0.5f * msToSmp;
            const float m = mix_.next();
            for (int ch = 0; ch < nCh; ++ch) {
                auto& line = lines_[static_cast<size_t>(ch)];
                const float x = buf.channels[ch][i];
                line.push(x);
                const double off = (ch & 1) ? 0.25 : 0.0;   // höger: 90° förskjuten
                const float d1 = base + dep * (1.0f + Lfo::sine(phase + off));
                const float d2 = base + dep * (1.0f + Lfo::sine(phase + off + 0.5));
                const float wet = 0.5f * (line.readHermite(d1) + line.readHermite(d2));
                buf.channels[ch][i] = x + m * (wet - x);
            }
        }
    }

private:
    static constexpr double kBaseMs = 7.0, kMaxDepthMs = 4.0;
    double sr_ = 48000.0;
    int numCh_ = 2;
    std::array<DelayLine, kMaxChannels> lines_;
    Lfo lfo_;
    SmoothedValue rate_, depth_, mix_;
};

} // namespace gfx
