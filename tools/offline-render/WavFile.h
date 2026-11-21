#pragma once
// Minimal WAV-läsare/skrivare utan beroenden (PCM 16/24/32 och float 32).

#include <cstdint>
#include <string>
#include <vector>

namespace gfx::wav {

struct Audio {
    int sampleRate = 48000;
    std::vector<std::vector<float>> channels; // [kanal][sample]
    int numSamples() const { return channels.empty() ? 0 : static_cast<int>(channels[0].size()); }
};

bool read(const std::string& path, Audio& out, std::string& error);
bool write(const std::string& path, const Audio& in, std::string& error); // float 32

} // namespace gfx::wav
