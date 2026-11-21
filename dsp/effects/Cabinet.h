#pragma once
// Kabinett-simulering med impulssvar (IR) och faltning utan latens.
//
// - Laddar valfri IR (t.ex. WAV från en IR-leverantör) via loadImpulseResponse().
//   IR:n samplas om till motorns frekvens, kortas till max kMaxIrSeconds och
//   normaliseras så att nivån blir jämn mellan olika IR-filer.
// - Utan laddad IR används ett inbyggt syntetiskt 1x12-liknande svar.
// - Byte av IR medan ljudet går är låsfritt: den nya IR:n byggs på anropande
//   tråd och lämnas över via en atomisk pekare. Den gamla tas om hand
//   (raderas) på icke-ljudtråd via collectGarbage() eller nästa laddning.

#include "../Effect.h"
#include "../core/Convolver.h"
#include "../core/FFT.h"
#include "../core/Parameter.h"
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

namespace gfx {

class Cabinet final : public Effect {
public:
    static constexpr double kMaxIrSeconds = 0.5;

    Parameter levelDb { 0.0f, -24.0f, 12.0f };

    Cabinet();
    ~Cabinet() override;

    std::string_view name() const override { return "Cabinet"; }
    void prepare(const ProcessSpec& spec) override;
    void reset() override;
    void process(AudioBufferView buf) override;

    // Ej realtid, valfri tråd. data = mono-IR (vid stereo: skicka vänster kanal).
    // Returnerar false om IR:n är tom eller ogiltig.
    bool loadImpulseResponse(const float* data, int numSamples, double sampleRate, std::string displayName);
    // Återgå till det inbyggda svaret.
    void useBuiltInImpulseResponse();
    // Namnet på aktiv IR (för GUI). Ej realtid.
    std::string impulseResponseName() const;
    // Radera utbytta IR:er. Anropa då och då från GUI-tråden (t.ex. timer).
    void collectGarbage();

    // Inbyggt svar vid 48 kHz (exponerad för tester/verktyg).
    static std::vector<float> builtInImpulseResponse();

private:
    ConvolverIR* buildForCurrentRate(); // kräver mutex_
    void publish(ConvolverIR* ir);       // kräver mutex_

    // Källa (icke-ljudtråd), skyddad av mutex_.
    mutable std::mutex mutex_;
    std::vector<float> sourceIr_;
    double sourceRate_ = 48000.0;
    std::string sourceName_;

    std::atomic<double> sampleRate_ { 48000.0 };
    FFT fft_;
    int maxParts_ = 1, numCh_ = 2;
    std::vector<Convolver> conv_;

    // Överlämning mellan trådar (se kommentar högst upp).
    ConvolverIR* current_ = nullptr;            // ägs av ljudtråden
    std::atomic<ConvolverIR*> pending_ { nullptr };
    std::atomic<ConvolverIR*> retired_ { nullptr };

    SmoothedValue level_;
};

} // namespace gfx
