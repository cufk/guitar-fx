#pragma once
// Tunn, icke-ägande vy över flerkanaliga ljudbuffrar.
// Gör att DSP-koden inte beror på JUCE och kan köras på iOS, Linux,
// Raspberry Pi eller mikrokontroller med samma kod.

#include <cassert>
#include <cstring>

namespace gfx {

constexpr int kMaxChannels = 8;

struct AudioBufferView {
    float* const* channels = nullptr;
    int numChannels = 0;
    int numSamples  = 0;

    float* channel(int ch) const {
        assert(ch >= 0 && ch < numChannels);
        return channels[ch];
    }

    void clear() const {
        for (int ch = 0; ch < numChannels; ++ch)
            std::memset(channels[ch], 0, sizeof(float) * static_cast<size_t>(numSamples));
    }

    // Delvy (t.ex. för att bearbeta i mindre block).
    AudioBufferView subBlock(int startSample, int length, float** scratchPtrs) const {
        for (int ch = 0; ch < numChannels; ++ch)
            scratchPtrs[ch] = channels[ch] + startSample;
        return { scratchPtrs, numChannels, length };
    }
};

} // namespace gfx
