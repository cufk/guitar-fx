#pragma once
// Små hjälpare för robust DSP: dB-konvertering, denormal-skydd och NaN-kontroll.

#include <cmath>
#include <cstdint>

#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__)
  #include <xmmintrin.h>
  #define GFX_X86 1
#endif

namespace gfx {

inline float dbToGain(float db) { return db <= -120.0f ? 0.0f : std::pow(10.0f, db * 0.05f); }
inline float gainToDb(float g)  { return g <= 1.0e-6f ? -120.0f : 20.0f * std::log10(g); }

// Sätter flush-to-zero / denormals-are-zero så länge objektet lever.
// Denormala tal (extremt små värden i reverb-svansar och filter) kan annars
// få CPU-lasten att skjuta i höjden och orsaka avbrott i ljudet.
class ScopedNoDenormals {
public:
    ScopedNoDenormals() noexcept {
#if defined(GFX_X86)
        previous_ = _mm_getcsr();
        _mm_setcsr(previous_ | 0x8040u); // FTZ | DAZ
#elif defined(__aarch64__)
        // Apple Silicon, iPhone/iPad och Raspberry Pi 4/5 (64-bit).
        uint64_t fpcr;
        asm volatile("mrs %0, fpcr" : "=r"(fpcr));
        previous_ = fpcr;
        asm volatile("msr fpcr, %0" : : "r"(fpcr | (1ull << 24))); // FZ
#endif
    }
    ~ScopedNoDenormals() noexcept {
#if defined(GFX_X86)
        _mm_setcsr(static_cast<unsigned int>(previous_));
#elif defined(__aarch64__)
        asm volatile("msr fpcr, %0" : : "r"(previous_));
#endif
    }
    ScopedNoDenormals(const ScopedNoDenormals&) = delete;
    ScopedNoDenormals& operator=(const ScopedNoDenormals&) = delete;
private:
    uint64_t previous_ = 0;
};

inline bool isFiniteSample(float x) { return std::isfinite(x); }

} // namespace gfx
