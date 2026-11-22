#pragma once
// Rig: grundljud + presets ("snapshots").
//
//  - Grundljudet (base) har ett komplett läge för varje effekt: på/av + alla värden.
//    Det är det som hörs när inget preset är aktivt – t.ex. EQ och lite reverb.
//  - Ett preset innehåller ett läge för en eller flera effekter. När presetet är
//    aktivt används dess läge för de effekterna; övriga effekter följer grundljudet.
//  - Ett preset i taget. select(i) på det redan aktiva = tillbaka till grundljudet.
//  - Redigering: GUI:t ändrar de levande parametrarna; syncFromLive() sparar i det som
//    är aktivt. Med ett preset aktivt tas en ändrad effekt automatiskt med i presetet
//    (grundljudet rörs inte); med grundljudet aktivt sparas ändringen där.
//    setIncludedInActive(id, false) tar bort en effekt ur presetet (följer grundljudet igen).
//
// Trådar: Rig används bara från meddelandetråden (GUI, MIDI-kö, state). Ljudtråden
// ser bara gfx::Parameter (atomic) och EffectChain::setEnabled (atomic), så byten
// glider mjukt via effekternas smoothing och kedjans crossfade/utklingning.

#include "EffectChain.h"
#include "dsp/core/Parameter.h"
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace gfx {

struct ParamSpec {
    std::string id;           // stabil nyckel (sparas i presets)
    std::string label;        // visningsnamn (UTF-8)
    std::string unit;         // t.ex. "Hz", "dB", "s", "ms" eller tomt
    Parameter* param = nullptr;
    float step = 0.01f;
    int decimals = 2;
    bool logScale = false;    // för frekvenser/tider: reglaget följer en logaritmisk skala
    std::vector<std::string> choices;  // icke-tom => val (värde = index)
    float defaultValue = 0.0f;         // fabriksvärde (dubbelklick i GUI:t återställer)
};

struct EffectSpec {
    std::string id, name;
    uint32_t colour = 0xffed5429;      // ARGB, egen färg per effekt i GUI:t
    int slot = -1;                     // plats i EffectChain
    std::vector<ParamSpec> params;
    std::function<float()> activity;   // valfri: t.ex. kompressorns nedreglering i dB (för GUI)
};

struct EffectState {
    bool enabled = false;
    std::map<std::string, float> values;
    bool operator==(const EffectState&) const = default;
};
using Snapshot = std::map<std::string, EffectState>;

struct Preset {
    std::string name;
    Snapshot overrides;   // bara effekterna som ingår i presetet
};

class Rig {
public:
    explicit Rig(EffectChain& chain) : chain_(chain) {}

    // --- Uppsättning (vid start) ---
    void addEffect(EffectSpec spec) { effects_.push_back(std::move(spec)); }
    const std::vector<EffectSpec>& effects() const { return effects_; }
    const EffectSpec* findEffect(const std::string& id) const;
    // Läs nuvarande levande värden som grundljud (anropas efter addEffect).
    void initBaseFromLive();

    // --- Val av preset ---
    int activePreset() const { return active_; }       // -1 = grundljud
    void select(int index);                             // samma som aktivt => grundljud
    void activate(int index);                           // exakt (ingen växling)
    void selectNext();                                  // grundljud -> 1 -> 2 ... -> grundljud
    void selectPrevious();

    // --- Redigering ---
    void syncFromLive();
    bool activeIncludes(const std::string& effectId) const;
    void setIncludedInActive(const std::string& effectId, bool include);

    // --- Hantera presets ---
    int  addPreset(const std::string& name);            // tomt preset, aktiveras inte
    void removePreset(int index);
    void renamePreset(int index, const std::string& name);
    int  duplicatePreset(int index);
    void movePreset(int from, int to);

    const std::vector<Preset>& presets() const { return presets_; }
    const Snapshot& base() const { return base_; }
    std::string activeName() const { return active_ >= 0 ? presets_[size_t(active_)].name : std::string(); }

    // Ersätt allt (vid laddning av sparat läge). Okända effekter/värden ignoreras,
    // saknade värden behåller nuvarande.
    void load(const Snapshot& base, const std::vector<Preset>& presets, int active);

    // Ökar vid varje ändring – för att GUI/sparning ska se att något hänt.
    uint64_t revision() const { return revision_; }

private:
    EffectState liveState(const EffectSpec& e) const;
    void apply(const EffectSpec& e, const EffectState& s);
    void applyAll();
    const EffectState* targetFor(const std::string& id) const;
    EffectState* mutableTargetFor(const std::string& id);
    bool valid(int index) const { return index >= 0 && index < static_cast<int>(presets_.size()); }

    EffectChain& chain_;
    std::vector<EffectSpec> effects_;
    Snapshot base_;
    std::vector<Preset> presets_;
    int active_ = -1;
    uint64_t revision_ = 0;
};

} // namespace gfx
