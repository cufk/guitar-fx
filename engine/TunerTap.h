#pragma once
// Avtappning för stämapparaten: ljudtråden skriver ingångssignalen (efter
// ingångsväljaren, före effekterna) till en ringbuffert; GUI-tråden läser de
// senaste samplesen och kör PitchDetector. Ljudtråden gör bara en kopiering –
// ingen allokering, inga lås. En läsning som överlappar en skrivning kan ge ett
// halvgammalt fönster, vilket är ofarligt för en stämapparat.
//
// "Tysta utgången" stänger av ljudet ut medan man stämmer (vanligt på scen).

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <vector>

namespace gfx {

class TunerTap {
public:
    static constexpr int kSize = 16384;   // > 0,3 s vid 48 kHz

    std::atomic<bool> active { false };   // skrivs bara när stämapparaten är öppen
    std::atomic<bool> muteOutput { true };

    TunerTap() : buf_(kSize, 0.0f) {}   // finns alltid – även om ljudkortet aldrig startar

    void prepare() { std::fill(buf_.begin(), buf_.end(), 0.0f); write_.store(0); }

    // Ljudtråden.
    void push(const float* x, int n) noexcept {
        if (!active.load(std::memory_order_relaxed) || buf_.empty()) return;
        uint32_t w = write_.load(std::memory_order_relaxed);
        for (int i = 0; i < n; ++i) buf_[(w + static_cast<uint32_t>(i)) & (kSize - 1)] = x[i];
        write_.store(w + static_cast<uint32_t>(n), std::memory_order_release);
    }

    bool shouldMute() const noexcept {
        return active.load(std::memory_order_relaxed) && muteOutput.load(std::memory_order_relaxed);
    }

    // GUI-tråden: kopiera de senaste n samplesen (n <= kSize).
    void latest(float* out, int n) const {
        const uint32_t w = write_.load(std::memory_order_acquire);
        n = std::min(n, kSize);
        if (buf_.empty()) { std::fill(out, out + n, 0.0f); return; }
        for (int i = 0; i < n; ++i) out[i] = buf_[(w - static_cast<uint32_t>(n) + static_cast<uint32_t>(i)) & (kSize - 1)];
    }

private:
    std::vector<float> buf_;
    std::atomic<uint32_t> write_ { 0 };
};

} // namespace gfx
