#pragma once
// Tremolo: volymen pulserar i en jämn takt.
//
// Form: 0 = mjuk (sinus), 1 = hård (nästan fyrkant, som en "hackig" vintage-tremolo) –
// mellanlägen blandar. Stereo: 0 = båda sidor samtidigt, 1 = vänster och höger i motfas
// (ljudet vandrar mellan högtalarna). Djup 0 = exakt genomgång.

#include "../Effect.h"
#include "../core/DelayLine.h"
#include "../core/Parameter.h"
#include <algorithm>
#include <cmath>

namespace gfx {

class Tremolo final : public Effect {
public:
    Parameter rateHz { 4.0f, 0.5f, 12.0f };
    Parameter depth  { 0.5f, 0.0f, 1.0f };
    Parameter shape  { 0.0f, 0.0f, 1.0f };
    Parameter stereo { 0.0f, 0.0f, 1.0f };

    std::string_view name() const override { return "Tremolo"; }

    void prepare(const ProcessSpec& spec) override {
        numCh_ = std::min(spec.numChannels, kMaxChannels);
        lfo_.setSampleRate(spec.sampleRate);
        rate_.prepare(spec.sampleRate, 0.05);
        depth_.prepare(spec.sampleRate, 0.03);
        shape_.prepare(spec.sampleRate, 0.05);
        stereo_.prepare(spec.sampleRate, 0.05);
        reset();
    }

    void reset() override {
        lfo_.reset();
        rate_.setCurrentAndTarget(rateHz.get());
        depth_.setCurrentAndTarget(depth.get());
        shape_.setCurrentAndTarget(shape.get());
        stereo_.setCurrentAndTarget(stereo.get());
    }

    void process(AudioBufferView buf) override {
        rate_.setTarget(rateHz.get());
        depth_.setTarget(depth.get());
        shape_.setTarget(shape.get());
        stereo_.setTarget(stereo.get());
        const int nCh = std::min(buf.numChannels, numCh_);
        for (int i = 0; i < buf.numSamples; ++i) {
            const double ph = lfo_.advance(rate_.next());
            const float dep = depth_.next(), sh = shape_.next(), st = stereo_.next();
            if (dep <= 0.0f) continue;
            for (int ch = 0; ch < nCh; ++ch) {
                const float s = Lfo::sine(ph + ((ch & 1) ? 0.5 * st : 0.0));
                const float u = 0.5f + 0.5f * ((1.0f - sh) * s + sh * std::tanh(4.0f * s) * kHard_);
                buf.channels[ch][i] *= 1.0f - dep * u;
            }
        }
    }

private:
    int numCh_ = 2;
    float kHard_ = 1.0f / std::tanh(4.0f);   // normerar den hårda formen till ±1
    Lfo lfo_;
    SmoothedValue rate_, depth_, shape_, stereo_;
};

} // namespace gfx
