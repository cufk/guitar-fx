#pragma once
// EQ för grundljudet (främst akustisk gitarr):
//   lågskärning -> bas (shelf 120 Hz) -> mitt (peak, valbar frekvens)
//   -> diskant (shelf 6 kHz) -> notch mot rundgång (smal, valbar frekvens)
//
// Notch-djup 0 dB = av. Vid rundgång: hitta tonen som tjuter (svep frekvensen
// med djup -20 dB tills det slutar), djupet behöver sällan vara mer.
// Koefficienter räknas om var 32:a sample medan värden glider (klickfritt).

#include "../Effect.h"
#include "../core/Biquad.h"
#include "../core/Parameter.h"
#include <algorithm>
#include <array>

namespace gfx {

class Eq final : public Effect {
public:
    Parameter lowCutHz   { 80.0f, 20.0f, 400.0f };
    Parameter lowGainDb  { 0.0f, -12.0f, 12.0f };
    Parameter midFreqHz  { 800.0f, 150.0f, 5000.0f };
    Parameter midGainDb  { 0.0f, -12.0f, 12.0f };
    Parameter highGainDb { 0.0f, -12.0f, 12.0f };
    Parameter notchHz    { 250.0f, 50.0f, 2000.0f };
    Parameter notchDepthDb { 0.0f, -30.0f, 0.0f };

    std::string_view name() const override { return "EQ"; }

    void prepare(const ProcessSpec& spec) override {
        sr_ = spec.sampleRate;
        numCh_ = std::min(spec.numChannels, kMaxChannels);
        for (auto& s : smooth_) s.prepare(sr_, 0.04);
        reset();
    }

    void reset() override {
        for (auto& f : filters_) f.reset();
        auto params = all();
        for (size_t i = 0; i < kNumParams; ++i) smooth_[i].setCurrentAndTarget(params[i]->get());
        updateCoeffs();
    }

    void process(AudioBufferView buf) override {
        auto params = all();
        bool changed = false;
        for (size_t i = 0; i < kNumParams; ++i) {
            smooth_[i].setTarget(params[i]->get());
            changed |= smooth_[i].isSmoothing();
        }
        const int nCh = std::min(buf.numChannels, numCh_);
        for (int start = 0; start < buf.numSamples; start += kSub) {
            const int n = std::min(kSub, buf.numSamples - start);
            if (changed) {
                for (auto& s : smooth_) for (int i = 0; i < n; ++i) s.next();
                updateCoeffs();
                changed = false;
                for (auto& s : smooth_) changed |= s.isSmoothing();
            }
            for (int ch = 0; ch < nCh; ++ch) {
                float* x = buf.channels[ch] + start;
                for (auto& f : filters_) f.process(ch, x, n);
            }
        }
    }

private:
    static constexpr size_t kNumParams = 7;
    static constexpr int kSub = 32;

    std::array<Parameter*, kNumParams> all() {
        return { &lowCutHz, &lowGainDb, &midFreqHz, &midGainDb, &highGainDb, &notchHz, &notchDepthDb };
    }

    void updateCoeffs() noexcept {
        auto v = [this](size_t i) { return static_cast<double>(smooth_[i].current()); };
        filters_[0].setCoeffs(BiquadCoeffs::highpass(sr_, v(0), 0.7071));
        filters_[1].setCoeffs(BiquadCoeffs::lowShelf(sr_, 120.0, v(1)));
        filters_[2].setCoeffs(BiquadCoeffs::peak(sr_, v(2), 1.0, v(3)));
        filters_[3].setCoeffs(BiquadCoeffs::highShelf(sr_, 6000.0, v(4)));
        filters_[4].setCoeffs(BiquadCoeffs::peak(sr_, v(5), 10.0, v(6)));
    }

    double sr_ = 48000.0;
    int numCh_ = 2;
    std::array<SmoothedValue, kNumParams> smooth_;
    std::array<Biquad, 5> filters_;
};

} // namespace gfx
