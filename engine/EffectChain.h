#pragma once
// Effektkedja med klickfri bypass per steg.
//
// Varje steg har en "mix" som glider mellan 0 (bypass) och 1 (helt på) på
// ~15 ms. Det gör att man kan trampa på/av effekter live utan klick.
//
// Uppsättning (add) görs innan prepare() och inte medan ljudet går.
// Omordning i realtid kommer i ett senare steg (kräver låsfri swap).

#include "dsp/Effect.h"
#include "dsp/core/Parameter.h"
#include <array>
#include <atomic>
#include <memory>
#include <vector>

namespace gfx {

class EffectChain {
public:
    static constexpr int kMaxSlots = 16;

    // Lägg till en effekt. Returnerar slot-index, eller -1 om kedjan är full.
    // Inte realtidssäker – anropas vid uppstart.
    int add(std::unique_ptr<Effect> effect, bool enabled = true);

    void prepare(const ProcessSpec& spec);
    void reset();
    void process(AudioBufferView buffer);

    void setEnabled(int slot, bool enabled);
    bool isEnabled(int slot) const;
    int  size() const { return numSlots_; }
    Effect* effect(int slot) const { return slots_[static_cast<size_t>(slot)].effect.get(); }

    int latencySamples() const;        // alla steg (för värdens latenskompensation)
    int activeLatencySamples() const;  // bara påslagna steg (för statusvisning)

private:
    struct Slot {
        std::unique_ptr<Effect> effect;
        std::atomic<bool> enabled { true };
        SmoothedValue mix;
        bool wasFullyOff = false;
        std::vector<std::vector<float>> dry; // scratch för crossfade
    };

    std::array<Slot, kMaxSlots> slots_;
    int numSlots_ = 0;
};

} // namespace gfx
