#pragma once
// Ljudmotorn: allt mellan ljudkortets ingång och utgång.
//
//   Ingång 1 / ingång 2 (t.ex. mikrofon + inbyggd mikrofon)
//     -> InputMixer (välj eller blanda, polaritet, tidsjustering) -> mono
//     -> Ingångsgain
//     -> Effektkedja (distortion, cab, chorus/flanger, reverb … läggs till här)
//     -> Utgångsgain
//     -> Säkerhetslimiter (alltid på, sist)
//     -> Ut (samma signal på alla utgångar tills stereoeffekter finns)
//
// Motorn vet inget om JUCE, CoreAudio eller ALSA. Samma klass används av
// Mac-appen, iOS-appen, offline-verktyget, testerna och senare Raspberry Pi.

#include "EffectChain.h"
#include "InputMixer.h"
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

    // Realtid. Kanal 0 och 1 i bufferten är ingång 1 och 2; InputMixer
    // väljer/blandar dem till mono. Resultatet skrivs till alla kanaler.
    // Klarar block som är större än maxBlockSize (delar upp internt).
    void process(AudioBufferView ioBuffer);

    void reset();

    // --- Kontroller (anropas från valfri tråd) ---
    Gain& inputGain()          { return inputGain_; }
    Gain& outputGain()         { return outputGain_; }
    SafetyLimiter& limiter()   { return limiter_; }
    EffectChain& chain()       { return chain_; }
    LatencyProbe& latencyProbe() { return probe_; }
    InputMixer& input()        { return input_; }

    // --- Mätvärden för GUI (läses från valfri tråd) ---
    // Topp per fysisk ingång (0 eller 1), före blandning – för att ställa gain på ljudkortet.
    float inputPeak(int ch) const { return inputPeak_[ch == 1 ? 1 : 0].load(std::memory_order_relaxed); }
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

    InputMixer input_;
    std::atomic<float> inputPeak_[2] { 0.0f, 0.0f }, outputPeak_ { 0.0f };
    std::atomic<uint32_t> recoveries_ { 0 };
    std::atomic<uint64_t> blocks_ { 0 };
};

} // namespace gfx
