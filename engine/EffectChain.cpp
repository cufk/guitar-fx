#include "EffectChain.h"
#include <algorithm>

namespace gfx {

int EffectChain::add(std::unique_ptr<Effect> effect, bool enabled) {
    if (numSlots_ >= kMaxSlots || !effect) return -1;
    auto& s = slots_[static_cast<size_t>(numSlots_)];
    s.effect = std::move(effect);
    s.enabled.store(enabled);
    s.mix.setCurrentAndTarget(enabled ? 1.0f : 0.0f);
    s.wasFullyOff = !enabled;
    return numSlots_++;
}

void EffectChain::prepare(const ProcessSpec& spec) {
    for (int i = 0; i < numSlots_; ++i) {
        auto& s = slots_[static_cast<size_t>(i)];
        s.effect->prepare(spec);
        s.mix.prepare(spec.sampleRate, 0.015);
        const bool on = s.enabled.load();
        s.mix.setCurrentAndTarget(on ? 1.0f : 0.0f);
        s.wasFullyOff = !on;
        s.dry.assign(static_cast<size_t>(spec.numChannels),
                     std::vector<float>(static_cast<size_t>(spec.maxBlockSize), 0.0f));
    }
}

void EffectChain::reset() {
    for (int i = 0; i < numSlots_; ++i)
        slots_[static_cast<size_t>(i)].effect->reset();
}

void EffectChain::setEnabled(int slot, bool enabled) {
    if (slot >= 0 && slot < numSlots_)
        slots_[static_cast<size_t>(slot)].enabled.store(enabled, std::memory_order_relaxed);
}

bool EffectChain::isEnabled(int slot) const {
    return slot >= 0 && slot < numSlots_
        && slots_[static_cast<size_t>(slot)].enabled.load(std::memory_order_relaxed);
}

int EffectChain::latencySamples() const {
    int total = 0;
    for (int i = 0; i < numSlots_; ++i)
        total += slots_[static_cast<size_t>(i)].effect->latencySamples();
    return total;
}

void EffectChain::process(AudioBufferView buf) {
    for (int i = 0; i < numSlots_; ++i) {
        auto& s = slots_[static_cast<size_t>(i)];
        s.mix.setTarget(s.enabled.load(std::memory_order_relaxed) ? 1.0f : 0.0f);

        // Helt avstängd: hoppa över (sparar CPU).
        if (!s.mix.isSmoothing() && s.mix.target() == 0.0f) {
            s.wasFullyOff = true;
            continue;
        }

        // På väg in från helt av: rensa gamla svansar så inget gammalt ljud "poppar" in.
        if (s.wasFullyOff) {
            s.effect->reset();
            s.wasFullyOff = false;
        }

        // Helt på: bearbeta direkt.
        if (!s.mix.isSmoothing() && s.mix.target() == 1.0f) {
            s.effect->process(buf);
            continue;
        }

        // Övergång: spara torr signal, bearbeta, blanda torr/våt med glidande mix.
        const int nCh = std::min(buf.numChannels, static_cast<int>(s.dry.size()));
        for (int ch = 0; ch < nCh; ++ch)
            std::copy(buf.channels[ch], buf.channels[ch] + buf.numSamples, s.dry[static_cast<size_t>(ch)].data());

        s.effect->process(buf);

        for (int n = 0; n < buf.numSamples; ++n) {
            const float m = s.mix.next();
            for (int ch = 0; ch < nCh; ++ch) {
                const float dry = s.dry[static_cast<size_t>(ch)][static_cast<size_t>(n)];
                buf.channels[ch][n] = dry + m * (buf.channels[ch][n] - dry);
            }
        }
    }
}

} // namespace gfx
