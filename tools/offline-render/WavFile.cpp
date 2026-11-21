#include "WavFile.h"
#include <cstring>
#include <fstream>

namespace gfx::wav {

namespace {
uint32_t u32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (uint32_t(p[3]) << 24); }
uint16_t u16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
void put32(std::ofstream& f, uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); }
void put16(std::ofstream& f, uint16_t v) { f.write(reinterpret_cast<const char*>(&v), 2); }
}

bool read(const std::string& path, Audio& out, std::string& error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { error = "Kan inte öppna " + path; return false; }
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), {});
    if (d.size() < 12 || std::memcmp(d.data(), "RIFF", 4) || std::memcmp(d.data() + 8, "WAVE", 4)) {
        error = "Inte en WAV-fil"; return false;
    }

    uint16_t format = 0, channels = 0, bits = 0;
    uint32_t rate = 0;
    const uint8_t* data = nullptr;
    uint32_t dataSize = 0;

    for (size_t pos = 12; pos + 8 <= d.size();) {
        const uint32_t size = u32(&d[pos + 4]);
        const uint8_t* body = &d[pos + 8];
        if (pos + 8 + size > d.size()) break;
        if (!std::memcmp(&d[pos], "fmt ", 4) && size >= 16) {
            format = u16(body); channels = u16(body + 2); rate = u32(body + 4); bits = u16(body + 14);
            if (format == 0xFFFE && size >= 26) format = u16(body + 24); // WAVE_FORMAT_EXTENSIBLE
        } else if (!std::memcmp(&d[pos], "data", 4)) {
            data = body; dataSize = size;
        }
        pos += 8 + size + (size & 1);
    }

    if (!data || channels == 0) { error = "Saknar fmt- eller data-chunk"; return false; }
    const bool isFloat = format == 3 && bits == 32;
    const bool isPcm = format == 1 && (bits == 16 || bits == 24 || bits == 32);
    if (!isFloat && !isPcm) { error = "Format som inte stöds (bitar=" + std::to_string(bits) + ")"; return false; }

    const int bytes = bits / 8;
    const size_t frames = dataSize / (static_cast<size_t>(bytes) * channels);
    out.sampleRate = static_cast<int>(rate);
    out.channels.assign(channels, std::vector<float>(frames));

    for (size_t i = 0; i < frames; ++i)
        for (int ch = 0; ch < channels; ++ch) {
            const uint8_t* p = data + (i * channels + static_cast<size_t>(ch)) * static_cast<size_t>(bytes);
            float v = 0.0f;
            if (isFloat)         std::memcpy(&v, p, 4);
            else if (bits == 16) v = static_cast<int16_t>(u16(p)) / 32768.0f;
            else if (bits == 24) v = static_cast<float>(static_cast<int32_t>((p[0] << 8) | (p[1] << 16) | (uint32_t(p[2]) << 24)) >> 8) / 8388608.0f;
            else                 v = static_cast<float>(static_cast<int32_t>(u32(p)) / 2147483648.0);
            out.channels[static_cast<size_t>(ch)][i] = v;
        }
    return true;
}

bool write(const std::string& path, const Audio& in, std::string& error) {
    std::ofstream f(path, std::ios::binary);
    if (!f) { error = "Kan inte skriva " + path; return false; }
    const uint16_t channels = static_cast<uint16_t>(in.channels.size());
    const uint32_t frames = static_cast<uint32_t>(in.numSamples());
    const uint32_t dataSize = frames * channels * 4;

    f.write("RIFF", 4); put32(f, 36 + dataSize); f.write("WAVE", 4);
    f.write("fmt ", 4); put32(f, 16); put16(f, 3); put16(f, channels);
    put32(f, static_cast<uint32_t>(in.sampleRate)); put32(f, static_cast<uint32_t>(in.sampleRate) * channels * 4);
    put16(f, static_cast<uint16_t>(channels * 4)); put16(f, 32);
    f.write("data", 4); put32(f, dataSize);
    for (uint32_t i = 0; i < frames; ++i)
        for (uint16_t ch = 0; ch < channels; ++ch)
            f.write(reinterpret_cast<const char*>(&in.channels[ch][i]), 4);
    return static_cast<bool>(f);
}

} // namespace gfx::wav
