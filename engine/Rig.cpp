#include "Rig.h"
#include <algorithm>

namespace gfx {

const EffectSpec* Rig::findEffect(const std::string& id) const {
    for (const auto& e : effects_) if (e.id == id) return &e;
    return nullptr;
}

EffectState Rig::liveState(const EffectSpec& e) const {
    EffectState s;
    s.enabled = chain_.isEnabled(e.slot);
    for (const auto& p : e.params) s.values[p.id] = p.param->get();
    return s;
}

void Rig::apply(const EffectSpec& e, const EffectState& s) {
    for (const auto& p : e.params) {
        auto it = s.values.find(p.id);
        if (it != s.values.end()) p.param->set(it->second);
    }
    chain_.setEnabled(e.slot, s.enabled);
}

const EffectState* Rig::targetFor(const std::string& id) const {
    if (valid(active_)) {
        const auto& o = presets_[size_t(active_)].overrides;
        if (auto it = o.find(id); it != o.end()) return &it->second;
    }
    if (auto it = base_.find(id); it != base_.end()) return &it->second;
    return nullptr;
}

EffectState* Rig::mutableTargetFor(const std::string& id) {
    return const_cast<EffectState*>(static_cast<const Rig*>(this)->targetFor(id));
}

void Rig::applyAll() {
    for (const auto& e : effects_)
        if (const auto* t = targetFor(e.id)) apply(e, *t);
}

void Rig::initBaseFromLive() {
    base_.clear();
    for (const auto& e : effects_) base_[e.id] = liveState(e);
    ++revision_;
}

void Rig::activate(int index) {
    active_ = valid(index) ? index : -1;
    applyAll();
    ++revision_;
}

void Rig::select(int index) {
    activate(index == active_ ? -1 : index);
}

void Rig::selectNext() {
    const int n = static_cast<int>(presets_.size());
    activate(active_ + 1 >= n ? -1 : active_ + 1);
}

void Rig::selectPrevious() {
    const int n = static_cast<int>(presets_.size());
    activate(active_ < 0 ? n - 1 : active_ - 1);
}

void Rig::syncFromLive() {
    // Det som ändras sparas i det som är aktivt: med ett preset aktivt tas en ändrad effekt
    // automatiskt med i presetet (grundljudet rörs inte). Med grundljudet aktivt sparas där.
    for (const auto& e : effects_) {
        const auto live = liveState(e);
        if (valid(active_)) {
            auto& o = presets_[size_t(active_)].overrides;
            if (auto it = o.find(e.id); it != o.end()) {
                if (!(live == it->second)) { it->second = live; ++revision_; }
                continue;
            }
            const auto b = base_.find(e.id);
            if (b != base_.end() && !(live == b->second)) { o[e.id] = live; ++revision_; }
            continue;
        }
        if (auto b = base_.find(e.id); b != base_.end() && !(live == b->second)) { b->second = live; ++revision_; }
    }
}

bool Rig::activeIncludes(const std::string& effectId) const {
    return valid(active_) && presets_[size_t(active_)].overrides.count(effectId) > 0;
}

void Rig::setIncludedInActive(const std::string& effectId, bool include) {
    if (!valid(active_)) return;
    const auto* e = findEffect(effectId);
    if (!e) return;
    auto& o = presets_[size_t(active_)].overrides;
    if (include) {
        // Börja från det som hörs nu, men påslagen – oftast vill man lägga till en effekt.
        auto s = liveState(*e);
        s.enabled = true;
        o[effectId] = s;
        apply(*e, s);
    } else {
        o.erase(effectId);
        if (auto it = base_.find(effectId); it != base_.end()) apply(*e, it->second);
    }
    ++revision_;
}

int Rig::addPreset(const std::string& name) {
    presets_.push_back({ name, {} });
    ++revision_;
    return static_cast<int>(presets_.size()) - 1;
}

void Rig::removePreset(int index) {
    if (!valid(index)) return;
    presets_.erase(presets_.begin() + index);
    if (active_ == index) { active_ = -1; applyAll(); }
    else if (active_ > index) --active_;
    ++revision_;
}

void Rig::renamePreset(int index, const std::string& name) {
    if (!valid(index)) return;
    presets_[size_t(index)].name = name;
    ++revision_;
}

int Rig::duplicatePreset(int index) {
    if (!valid(index)) return -1;
    Preset copy = presets_[size_t(index)];
    copy.name += " (kopia)";
    presets_.insert(presets_.begin() + index + 1, copy);
    if (active_ > index) ++active_;
    ++revision_;
    return index + 1;
}

void Rig::movePreset(int from, int to) {
    if (!valid(from) || !valid(to) || from == to) return;
    Preset p = presets_[size_t(from)];
    presets_.erase(presets_.begin() + from);
    presets_.insert(presets_.begin() + to, p);
    if (active_ == from) active_ = to;
    else if (from < active_ && to >= active_) --active_;
    else if (from > active_ && to <= active_) ++active_;
    ++revision_;
}

void Rig::load(const Snapshot& base, const std::vector<Preset>& presets, int active) {
    // Grundljud: börja från nuvarande läge och skriv över det som finns sparat.
    for (const auto& e : effects_) {
        auto& b = base_[e.id];
        if (b.values.empty()) b = liveState(e);
        if (auto it = base.find(e.id); it != base.end()) {
            b.enabled = it->second.enabled;
            for (const auto& [k, v] : it->second.values)
                if (b.values.count(k)) b.values[k] = v;
        }
    }
    // Presets: behåll bara kända effekter; saknade värden fylls från grundljudet.
    presets_.clear();
    for (const auto& p : presets) {
        Preset q { p.name, {} };
        for (const auto& [id, st] : p.overrides) {
            if (!findEffect(id)) continue;
            EffectState s = base_[id];
            s.enabled = st.enabled;
            for (const auto& [k, v] : st.values)
                if (s.values.count(k)) s.values[k] = v;
            q.overrides[id] = s;
        }
        presets_.push_back(std::move(q));
    }
    active_ = valid(active) ? active : -1;
    applyAll();
    ++revision_;
}

} // namespace gfx
