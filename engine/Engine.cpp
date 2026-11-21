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
}

void Engine::reset() {
    inputGain_.reset();
    chain_.reset();
    outputGain_.reset();
    limiter_.reset();
}

void Engine::process(AudioBufferView io) {
    ScopedNoDenormals noDenormals;

    if (io.numChannels <= 0 || io.numSamples <= 0) return;
    const int channels = std::min(io.numChannels, spec_.numChannels);

    // 1. Mono-källa -> alla kanaler (dual mono).
    const int src = std::clamp(inputChannel.load(std::memory_order_relaxed), 0, io.numChannels - 1);
    float inPeak = 0.0f;
    for (int n = 0; n < io.numSamples; ++n)
        inPeak = std::max(inPeak, std::fabs(io.channels[src][n]));
    for (int ch = 0; ch < io.numChannels; ++ch)
        if (ch != src)
            std::memcpy(io.channels[ch], io.channels[src], sizeof(float) * static_cast<size_t>(io.numSamples));
    inputPeak_.store(inPeak, std::memory_order_relaxed);

    // Kanaler utöver vad vi förberett för tystas (ska inte hända, men säkert).
    for (int ch = channels; ch < io.numChannels; ++ch)
        std::memset(io.channels[ch], 0, sizeof(float) * static_cast<size_t>(io.numSamples));

    // 2. Bearbeta i block om högst maxBlockSize.
    float* ptrs[kMaxChannels];
    const int usedCh = std::min(channels, kMaxChannels);
    AudioBufferView view { io.channels, usedCh, io.numSamples };
    for (int start = 0; start < io.numSamples; start += spec_.maxBlockSize) {
        const int len = std::min(spec_.maxBlockSize, io.numSamples - start);
        processChunk(view.subBlock(start, len, ptrs));
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
