#pragma once
// Sustain: kompressor inställd för att låta toner klinga längre.
//
//  - "Sustain" (0..1) styr både tröskel (-12 .. -42 dB) och ratio (1,5:1 .. 10:1),
//    med automatisk utjämning av nivån (makeup).
//  - "Anslag" (attack) avgör hur mycket av anslaget som slipper igenom innan
//    kompressorn tar tag – längre tid = mer naturligt plock.
//  - "Mix" blandar in torr signal (parallellkompression): behåller dynamiken i
//    anslaget men ger längre ton – särskilt bra på akustisk gitarr.
//  - Under -60 dBFS dras förstärkningen ner (expander 2:1) så att brus och
//    rundgångsrisk inte förstärks i pauser.
// Detektorn är länkad över kanalerna (stereobilden bevaras).

#include "../Effect.h"
#include "../core/DspUtils.h"
#include "../core/Parameter.h"
#include <algorithm>
#include <atomic>
#include <cmath>

namespace gfx {

class Sustain final : public Effect {
public:
    Parameter amount    { 0.5f, 0.0f, 1.0f };
    Parameter attackMs  { 12.0f, 1.0f, 80.0f };
    Parameter releaseMs { 300.0f, 50.0f, 1500.0f };
    Parameter levelDb   { 0.0f, -12.0f, 12.0f };
    Parameter mix       { 1.0f, 0.0f, 1.0f };

    std::string_view name() const override { return "Sustain"; }

    void prepare(const ProcessSpec& spec) override {
        sr_ = spec.sampleRate;
        numCh_ = std::min(spec.numChannels, kMaxChannels);
        amount_.prepare(sr_, 0.05);
        level_.prepare(sr_, 0.05);
        mix_.prepare(sr_, 0.03);
        reset();
    }

    void reset() override {
        env_ = 0.0f;
        amount_.setCurrentAndTarget(amount.get());
        level_.setCurrentAndTarget(levelDb.get());
        mix_.setCurrentAndTarget(mix.get());
        gr_.store(0.0f, std::memory_order_relaxed);
    }

    void process(AudioBufferView buf) override {
        amount_.setTarget(amount.get());
        level_.setTarget(levelDb.get());
        mix_.setTarget(mix.get());
        const float atk = coeff(attackMs.get()), rel = coeff(releaseMs.get());
        const int nCh = std::min(buf.numChannels, numCh_);
        float minGr = 0.0f;

        for (int i = 0; i < buf.numSamples; ++i) {
            float peak = 0.0f;
            for (int ch = 0; ch < nCh; ++ch) peak = std::max(peak, std::fabs(buf.channels[ch][i]));
            if (!(peak < 1.0e6f)) peak = 1.0e6f;   // NaN/Inf-skydd för detektorn
            env_ = peak + (peak > env_ ? atk : rel) * (env_ - peak);

            const float a = amount_.next();
            const float thr = -12.0f - 30.0f * a;
            const float ratio = 1.5f + 8.5f * a;
            const float lvl = gainToDb(env_);
            const float grDb = gainComputer(lvl, thr, ratio);
            const float makeup = -thr * (1.0f - 1.0f / ratio) * 0.6f;
            const float g = dbToGain(grDb + makeup + level_.next());
            minGr = std::min(minGr, grDb);

            const float m = mix_.next();
            for (int ch = 0; ch < nCh; ++ch) {
                const float x = buf.channels[ch][i];
                buf.channels[ch][i] = x + m * (x * g - x);
            }
        }
        gr_.store(minGr, std::memory_order_relaxed);
    }

    // För GUI: hur mycket kompressorn drog ner i senaste blocket (dB, <= 0).
    float gainReductionDb() const { return gr_.load(std::memory_order_relaxed); }

    // Exponerad för tester: nedreglering i dB för en nivå (utan makeup).
    static float gainComputer(float levelDb, float thr, float ratio) noexcept {
        constexpr float knee = 8.0f;
        float gr = 0.0f;
        const float over = levelDb - thr;
        if (over >= knee * 0.5f) gr = over * (1.0f / ratio - 1.0f);
        else if (over > -knee * 0.5f) {
            const float x = over + knee * 0.5f;
            gr = (1.0f / ratio - 1.0f) * x * x / (2.0f * knee);
        }
        if (levelDb < -60.0f) gr += (levelDb + 60.0f);   // expander 2:1 under -60 dBFS
        return std::max(gr, -60.0f);
    }

private:
    float coeff(float ms) const noexcept { return static_cast<float>(std::exp(-1.0 / (ms * 1e-3 * sr_))); }

    double sr_ = 48000.0;
    int numCh_ = 2;
    float env_ = 0.0f;
    SmoothedValue amount_, level_, mix_;
    std::atomic<float> gr_ { 0.0f };
};

} // namespace gfx
