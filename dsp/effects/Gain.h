#pragma once
// Enkel gain-effekt i dB med mjuk övergång. Används för in- och utnivå.

#include "../Effect.h"
#include "../core/DspUtils.h"
#include "../core/Parameter.h"

namespace gfx {

class Gain final : public Effect {
public:
    explicit Gain(float defaultDb = 0.0f, float minDb = -60.0f, float maxDb = 24.0f)
        : gainDb(defaultDb, minDb, maxDb) {}

    Parameter gainDb;

    std::string_view name() const override { return "Gain"; }

    void prepare(const ProcessSpec& spec) override {
        smoother_.prepare(spec.sampleRate, 0.02);
        smoother_.setCurrentAndTarget(dbToGain(gainDb.get()));
    }

    void reset() override { smoother_.setCurrentAndTarget(dbToGain(gainDb.get())); }

    void process(AudioBufferView buf) override {
        smoother_.setTarget(dbToGain(gainDb.get()));
        for (int i = 0; i < buf.numSamples; ++i) {
            const float g = smoother_.next();
            for (int ch = 0; ch < buf.numChannels; ++ch)
                buf.channels[ch][i] *= g;
        }
    }

private:
    SmoothedValue smoother_;
};

} // namespace gfx
