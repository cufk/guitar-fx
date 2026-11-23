#pragma once
// Kroppsklang: gör den inbyggda pickupens hårda, "plastiga" ljud mer likt en mikrofon
// framför gitarren.
//
// En piezo-pickup hör strängen men knappt kroppen: luftresonansen (~90–130 Hz) och
// lockets resonans (~180–270 Hz) saknas, och 1,5–2,5 kHz blir "kvackig". Kroppen läggs
// till med smala resonansfilter (de ringer ut som en kropp) plus en bred sänkning av
// kvacket och lite luft i diskanten. Fyra kroppstyper med olika resonanser.
//
// Mängd 0 och luft 0 = exakt genomgång. Ingen latens (inga IR-filer behövs).

#include "../Effect.h"
#include "../core/Biquad.h"
#include "../core/Parameter.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace gfx {

class Body final : public Effect {
public:
    static constexpr int kTypes = 4, kBands = 5;

    Parameter type   { 0.0f, 0.0f, 3.0f };   // 0 Dreadnought, 1 Jumbo, 2 Auditorium, 3 Parlor
    Parameter amount { 0.6f, 0.0f, 1.0f };
    Parameter airDb  { 2.0f, 0.0f, 6.0f };

    static const char* typeName(int t) {
        static constexpr const char* n[kTypes] = { "Dreadnought", "Jumbo", "Auditorium", "Parlor" };
        return n[std::clamp(t, 0, kTypes - 1)];
    }

    std::string_view name() const override { return "Body"; }

    void prepare(const ProcessSpec& spec) override {
        sr_ = spec.sampleRate;
        numCh_ = std::min(spec.numChannels, kMaxChannels);
        amount_.prepare(sr_, 0.05);
        air_.prepare(sr_, 0.05);
        reset();
    }

    void reset() override {
        for (auto& b : bands_) b.reset();
        airShelf_.reset();
        amount_.setCurrentAndTarget(amount.get());
        air_.setCurrentAndTarget(airDb.get());
        appliedType_ = -1;
        update(true);
    }

    void process(AudioBufferView buf) override {
        amount_.setTarget(amount.get());
        air_.setTarget(airDb.get());
        // Koefficienter per block (mängd och luft glider via smoothing).
        for (int i = 0; i < buf.numSamples; ++i) { amount_.next(); air_.next(); }
        update(false);
        if (bypass_) return;
        const int nCh = std::min(buf.numChannels, numCh_);
        for (int ch = 0; ch < nCh; ++ch) {
            for (auto& b : bands_) b.process(ch, buf.channels[ch], buf.numSamples);
            airShelf_.process(ch, buf.channels[ch], buf.numSamples);
        }
    }

private:
    struct Band { float hz, q, db; };
    static const Band& band(int t, int k) {
        // Resonanser och korrigeringar per kroppstyp (förstärkning vid mängd 1).
        static constexpr Band b[kTypes][kBands] = {
            { { 98, 5.0f, 7.0f }, { 200, 4.0f, 4.0f }, { 420, 1.2f, -3.0f }, { 1900, 1.0f, -4.5f }, { 3500, 2.0f, 1.5f } },  // Dreadnought
            { { 88, 5.0f, 8.0f }, { 180, 4.0f, 5.0f }, { 380, 1.2f, -3.5f }, { 1700, 1.0f, -4.0f }, { 3200, 2.0f, 1.0f } },  // Jumbo
            { { 108, 5.0f, 5.5f }, { 230, 4.0f, 3.5f }, { 520, 1.2f, -2.0f }, { 2100, 1.0f, -3.5f }, { 4000, 2.0f, 2.0f } }, // Auditorium
            { { 130, 4.5f, 4.0f }, { 270, 4.0f, 3.0f }, { 800, 1.5f, 1.5f }, { 2400, 1.0f, -3.0f }, { 4500, 2.0f, 2.0f } },  // Parlor
        };
        return b[std::clamp(t, 0, kTypes - 1)][std::clamp(k, 0, kBands - 1)];
    }

    void update(bool force) {
        const int t = std::clamp(static_cast<int>(std::lround(type.get())), 0, kTypes - 1);
        const float a = amount_.current(), air = air_.current();
        if (!force && t == appliedType_ && a == appliedAmount_ && air == appliedAir_) return;
        appliedType_ = t; appliedAmount_ = a; appliedAir_ = air;
        bypass_ = a <= 1.0e-4f && air <= 1.0e-3f;
        for (int k = 0; k < kBands; ++k) {
            const auto& b = band(t, k);
            bands_[size_t(k)].setCoeffs(BiquadCoeffs::peak(sr_, b.hz, b.q, b.db * a));
        }
        airShelf_.setCoeffs(BiquadCoeffs::highShelf(sr_, 7000.0, air));
        if (bypass_) { for (auto& b : bands_) b.reset(); airShelf_.reset(); }
    }

    double sr_ = 48000.0;
    int numCh_ = 2;
    std::array<Biquad, kBands> bands_;
    Biquad airShelf_;
    SmoothedValue amount_, air_;
    int appliedType_ = -1;
    float appliedAmount_ = -1.0f, appliedAir_ = -1.0f;
    bool bypass_ = false;
};

} // namespace gfx
