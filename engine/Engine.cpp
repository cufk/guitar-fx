#include "Engine.h"
#include "dsp/core/DspUtils.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace gfx {

Engine::Engine() = default;

void Engine::prepare(const ProcessSpec& spec) {
    spec_ = spec;
    inputGain_.prepare(spec);
    chain_.prepare(spec);
    outputGain_.prepare(spec);
    limiter_.prepare(spec);
    probe_.prepare(spec.sampleRate);
    input_.prepare(spec.sampleRate);
}

void Engine::reset() {
    input_.reset();
    inputGain_.reset();
    chain_.reset();
    outputGain_.reset();
    limiter_.reset();
}

void Engine::process(AudioBufferView io) {
    ScopedNoDenormals noDenormals;

    if (io.numChannels <= 0 || io.numSamples <= 0) return;
    const int channels = std::min(io.numChannels, spec_.numChannels);

    // 1. Ingångar -> mono (kanal 0) -> alla kanaler (dual mono).
    const bool hasIn2 = io.numChannels > 1;
    for (int ch = 0; ch < (hasIn2 ? 2 : 1); ++ch) {
        float peak = 0.0f;
        for (int n = 0; n < io.numSamples; ++n) peak = std::max(peak, std::fabs(io.channels[ch][n]));
        inputPeak_[ch].store(peak, std::memory_order_relaxed);
    }
    if (!hasIn2) inputPeak_[1].store(0.0f, std::memory_order_relaxed);

    // Latensmätning: proben äger utgången (klick ut, lyssnar på ingången).
    // Effektkedjan och ingångsblandningen hoppas över, men limitern körs alltid.
    const int probeSrc = (hasIn2 && input_.mode.load(std::memory_order_relaxed) == InputMixer::Input2) ? 1 : 0;
    const bool probing = probe_.process(io.channels[probeSrc], io.channels[0], io.numSamples);
    if (!probing)
        input_.process(io.channels[0], hasIn2 ? io.channels[1] : nullptr, io.channels[0], io.numSamples);

    for (int ch = 1; ch < io.numChannels; ++ch)
        std::memcpy(io.channels[ch], io.channels[0], sizeof(float) * static_cast<size_t>(io.numSamples));

    // Kanaler utöver vad vi förberett för tystas (ska inte hända, men säkert).
    for (int ch = channels; ch < io.numChannels; ++ch)
        std::memset(io.channels[ch], 0, sizeof(float) * static_cast<size_t>(io.numSamples));

    // 2. Bearbeta i block om högst maxBlockSize.
    float* ptrs[kMaxChannels];
    const int usedCh = std::min(channels, kMaxChannels);
    AudioBufferView view { io.channels, usedCh, io.numSamples };
    for (int start = 0; start < io.numSamples; start += spec_.maxBlockSize) {
        const int len = std::min(spec_.maxBlockSize, io.numSamples - start);
        auto chunk = view.subBlock(start, len, ptrs);
        if (probing) limiter_.process(chunk);
        else         processChunk(chunk);
    }

    // 3. Mätare.
    float outPeak = 0.0f;
    for (int ch = 0; ch < usedCh; ++ch)
        for (int n = 0; n < io.numSamples; ++n)
            outPeak = std::max(outPeak, std::fabs(io.channels[ch][n]));
    outputPeak_.store(outPeak, std::memory_order_relaxed);
    blocks_.fetch_add(1, std::memory_order_relaxed);
}

void Engine::processChunk(AudioBufferView buf) {
    inputGain_.process(buf);
    chain_.process(buf);
    outputGain_.process(buf);
    limiter_.process(buf);

    // Om någon effekt producerat NaN/Inf har limitern redan tystat det.
    // Återställ hela kedjan så att trasigt internt tillstånd (t.ex. en
    // reverb-svans full av NaN) inte fortsätter att påverka ljudet.
    if (limiter_.consumeNonFiniteFlag()) {
        chain_.reset();
        recoveries_.fetch_add(1, std::memory_order_relaxed);
    }
}

} // namespace gfx
