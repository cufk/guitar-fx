#pragma once
// Volym per preset + solo-boost. Ligger sist i kedjan (före utnivå och limiter).
//
// - levelDb sparas per preset, så att alla låtar låter lika starkt i PA:t.
// - boostDb är hur mycket boosten höjer; själva boosten (på/av) är global och styrs med
//   pedal/tangent/knapp via setBoost() – den följer alltså inte med vid preset-byte.
// - Mjuk övergång (30 ms) så att boost och byten inte klickar. Limitern skyddar utgången.

#include "../Effect.h"
#include "../core/DspUtils.h"
#include "../core/Parameter.h"
#include <atomic>

namespace gfx {

class Volume final : public Effect {
public:
    Parameter levelDb { 0.0f, -12.0f, 6.0f };
    Parameter boostDb { 4.0f, 0.0f, 12.0f };

    std::string_view name() const override { return "Volume"; }

    void setBoost(bool on) noexcept { boost_.store(on, std::memory_order_relaxed); }
    bool boostOn() const noexcept { return boost_.load(std::memory_order_relaxed); }

    void prepare(const ProcessSpec& spec) override {
        gain_.prepare(spec.sampleRate, 0.03);
        reset();
    }

    void reset() override { gain_.setCurrentAndTarget(targetGain()); }

    void process(AudioBufferView buf) override {
        gain_.setTarget(targetGain());
        if (!gain_.isSmoothing() && gain_.target() == 1.0f) return;   // exakt genomgång vid 0 dB
        for (int i = 0; i < buf.numSamples; ++i) {
            const float g = gain_.next();
            for (int ch = 0; ch < buf.numChannels; ++ch) buf.channels[ch][i] *= g;
        }
    }

private:
    float targetGain() const { return dbToGain(levelDb.get() + (boostOn() ? boostDb.get() : 0.0f)); }
    std::atomic<bool> boost_ { false };
    SmoothedValue gain_;
};

} // namespace gfx
