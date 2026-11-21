#pragma once
// Ljudmotorn: allt mellan ljudkortets ingång och utgång.
//
//   Gitarr (mono, vald ingång)
//     -> Ingångsgain
//     -> Effektkedja (distortion, cab, chorus/flanger, reverb … läggs till här)
//     -> Utgångsgain
//     -> Säkerhetslimiter (alltid på, sist)
//     -> Ut (samma signal på alla utgångar tills stereoeffekter finns)
//
// Motorn vet inget om JUCE, CoreAudio eller ALSA. Samma klass används av
// Mac-appen, iOS-appen, offline-verktyget, testerna och senare Raspberry Pi.

#include "EffectChain.h"
#include "LatencyProbe.h"
#include "dsp/effects/Gain.h"
#include "dsp/effects/SafetyLimiter.h"
#include <atomic>
#include <cstdint>

namespace gfx {

class Engine {
public:
    Engine();

    // Ej realtid. Anropas när samplingsfrekvens/buffert ändras.
    void prepare(const ProcessSpec& spec);

    // Realtid. Kanal `inputChannel` i bufferten är gitarrsignalen (mono).
    // Resultatet skrivs till alla kanaler i bufferten.
    // Klarar block som är större än maxBlockSize (delar upp internt).
    void process(AudioBufferView ioBuffer);

    void reset();

    // --- Kontroller (anropas från valfri tråd) ---
    Gain& inputGain()          { return inputGain_; }
    Gain& outputGain()         { return outputGain_; }
    SafetyLimiter& limiter()   { return limiter_; }
    EffectChain& chain()       { return chain_; }
    LatencyProbe& latencyProbe() { return probe_; }
    std::atomic<int> inputChannel { 0 };

    // --- Mätvärden för GUI (läses från valfri tråd) ---
    float inputPeak()  const { return inputPeak_.load(std::memory_order_relaxed); }
    float outputPeak() const { return outputPeak_.load(std::memory_order_relaxed); }
    float limiterGainReductionDb() const { return limiter_.gainReductionDb(); }
    uint32_t recoveryCount() const { return recoveries_.load(std::memory_order_relaxed); }
    uint64_t processedBlocks() const { return blocks_.load(std::memory_order_relaxed); }

    int latencySamples() const { return chain_.latencySamples(); }
    const ProcessSpec& spec() const { return spec_; }

private:
    void processChunk(AudioBufferView buf);

    ProcessSpec spec_;
    Gain inputGain_  { 0.0f, -24.0f, 24.0f };
    Gain outputGain_ { 0.0f, -60.0f, 12.0f };
    EffectChain chain_;
    SafetyLimiter limiter_;
    LatencyProbe probe_;

    std::atomic<float> inputPeak_ { 0.0f }, outputPeak_ { 0.0f };
    std::atomic<uint32_t> recoveries_ { 0 };
    std::atomic<uint64_t> blocks_ { 0 };
};

} // namespace gfx
