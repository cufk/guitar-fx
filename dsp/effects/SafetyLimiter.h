#pragma once
// Säkerhetslimiter – sista steget i kedjan, kan inte förbikopplas.
//
// Syfte: skydda PA, öron och högtalare. Oavsett vad en effekt gör (feedback
// i flanger/reverb, bugg, NaN) ska utsignalen aldrig överstiga taket.
//
//  1. NaN/Inf-skydd: icke-ändliga samples ersätts med 0 och flaggas så att
//     motorn kan återställa kedjan.
//  2. Peak-limiter med snabb attack och mjuk release, länkad över kanaler.
//  3. Hård klippning vid taket som sista garanti (fångar transienter som
//     limitern inte hinner med eftersom den saknar lookahead = ingen latens).

#include "../Effect.h"
#include "../core/DspUtils.h"
#include "../core/Parameter.h"
#include <algorithm>
#include <atomic>
#include <cmath>

namespace gfx {

class SafetyLimiter final : public Effect {
public:
    Parameter ceilingDb { -1.0f, -24.0f, 0.0f };

    std::string_view name() const override { return "SafetyLimiter"; }

    void prepare(const ProcessSpec& spec) override {
        const double sr = spec.sampleRate;
        attackCoeff_  = static_cast<float>(std::exp(-1.0 / (0.0005 * sr))); // 0.5 ms
        releaseCoeff_ = static_cast<float>(std::exp(-1.0 / (0.150  * sr))); // 150 ms
        reset();
    }

    void reset() override {
        envelope_ = 1.0f;
        minGainThisBlock_.store(1.0f, std::memory_order_relaxed);
    }

    void process(AudioBufferView buf) override {
        const float ceiling = dbToGain(ceilingDb.get());
        float minGain = 1.0f;
        bool hadNonFinite = false;

        for (int i = 0; i < buf.numSamples; ++i) {
            // Länkad peak över alla kanaler, så stereobilden bevaras.
            float peak = 0.0f;
            for (int ch = 0; ch < buf.numChannels; ++ch) {
                float& s = buf.channels[ch][i];
                if (!isFiniteSample(s)) { s = 0.0f; hadNonFinite = true; }
                peak = std::max(peak, std::fabs(s));
            }

            const float wanted = peak > ceiling ? ceiling / peak : 1.0f;
            const float coeff  = wanted < envelope_ ? attackCoeff_ : releaseCoeff_;
            envelope_ = wanted + coeff * (envelope_ - wanted);
            const float g = envelope_;
            minGain = std::min(minGain, g);

            for (int ch = 0; ch < buf.numChannels; ++ch) {
                float s = buf.channels[ch][i] * g;
                buf.channels[ch][i] = std::clamp(s, -ceiling, ceiling); // sista garanti
            }
        }

        minGainThisBlock_.store(minGain, std::memory_order_relaxed);
        if (hadNonFinite) nonFiniteDetected_.store(true, std::memory_order_relaxed);
    }

    // För mätare i GUI: hur mycket limitern drog ner i senaste blocket (dB, <= 0).
    float gainReductionDb() const { return gainToDb(minGainThisBlock_.load(std::memory_order_relaxed)); }

    // Returnerar true en gång om NaN/Inf har upptäckts sedan senaste anropet.
    bool consumeNonFiniteFlag() { return nonFiniteDetected_.exchange(false, std::memory_order_relaxed); }

private:
    float attackCoeff_ = 0.0f, releaseCoeff_ = 0.0f;
    float envelope_ = 1.0f;
    std::atomic<float> minGainThisBlock_ { 1.0f };
    std::atomic<bool>  nonFiniteDetected_ { false };
};

} // namespace gfx
