#pragma once
// Rundgångsskydd: hittar toner som ringer upp (rundgång) och lägger in smala notch-filter.
//
// Analys på ljudtråden, utan allokering: 4096-punkters FFT var 1024:e sample (≈21 ms vid
// 48 kHz) på mono-summan EFTER filtren. En topp räknas som rundgång när den
//   1. sticker upp kraftigt över sin omgivning (topp mot medel, PAPR),
//   2. saknar övertoner och inte själv är en överton (jämförs med 2f, 3f, f/2 och f/3) –
//      en gitarrton har starka övertoner, rundgång är nästan en ren sinus,
//   3. ligger kvar utan att klinga av i flera analysfönster i rad.
// Då läggs ett smalt filter (Q 30, −9 dB) på frekvensen. Ringer den kvar fördjupas filtret
// i steg om 3 dB ned till maxdjupet. Filtren ligger kvar tills de nollställs (requestReset)
// eller platserna tar slut – då ersätts det äldsta. Ingen latens: analysen sker vid sidan av.
// Känsligheten styr trösklarna och hur länge en ton måste ringa innan filtret slår till.

#include "../Effect.h"
#include "../core/Biquad.h"
#include "../core/FFT.h"
#include "../core/Parameter.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace gfx {

class FeedbackSuppressor final : public Effect {
public:
    static constexpr int kMaxFilters = 8, kFftSize = 4096, kHop = 1024;

    Parameter sensitivity { 0.5f, 0.0f, 1.0f };
    Parameter maxFilters  { 6.0f, 1.0f, 8.0f };
    Parameter maxDepthDb  { -18.0f, -30.0f, -6.0f };

    std::string_view name() const override { return "Feedback"; }

    // Valfri tråd: töm alla filter (t.ex. efter soundcheck eller byte av lokal).
    void requestReset() noexcept { resetRequested_.store(true, std::memory_order_relaxed); }
    // För GUI: antal aktiva filter och deras frekvenser (0 = ledig plats).
    int activeFilters() const noexcept { return active_.load(std::memory_order_relaxed); }
    float filterHz(int i) const noexcept { return i >= 0 && i < kMaxFilters ? hzOut_[size_t(i)].load(std::memory_order_relaxed) : 0.0f; }
    float filterDepthDb(int i) const noexcept { return i >= 0 && i < kMaxFilters ? depthOut_[size_t(i)].load(std::memory_order_relaxed) : 0.0f; }

    void prepare(const ProcessSpec& spec) override {
        sr_ = spec.sampleRate;
        numCh_ = std::min(spec.numChannels, kMaxChannels);
        fft_.prepare(kFftSize);
        ring_.assign(size_t(kFftSize), 0.0f);
        frame_.assign(size_t(kFftSize), {});
        magDb_.assign(size_t(kFftSize / 2 + 1), -200.0f);
        prefix_.assign(size_t(kFftSize / 2 + 2), 0.0);
        window_.resize(size_t(kFftSize));
        for (int i = 0; i < kFftSize; ++i)
            window_[size_t(i)] = static_cast<float>(0.5 - 0.5 * std::cos(2.0 * 3.14159265358979 * i / kFftSize));
        for (auto& s : slots_) s = {};
        frameNo_ = 0;
        reset();
    }

    // Nollställer bara filtrens tillstånd och analysbufferten – inlärda filter ligger kvar.
    void reset() override {
        std::fill(ring_.begin(), ring_.end(), 0.0f);
        writePos_ = 0;
        hopCount_ = 0;
        for (auto& s : slots_) s.bq.reset();
        for (auto& t : trackers_) t = {};
    }

    void process(AudioBufferView buf) override {
        if (resetRequested_.exchange(false, std::memory_order_relaxed)) {
            for (auto& s : slots_) s = {};
            for (auto& t : trackers_) t = {};
            publish();
        }
        updateFilters();
        const int nCh = std::min(buf.numChannels, numCh_);
        for (auto& s : slots_) {
            if (!s.used) continue;
            for (int ch = 0; ch < nCh; ++ch) s.bq.process(ch, buf.channels[ch], buf.numSamples);
        }
        // Analysera det som faktiskt går ut (efter filtren).
        const float norm = nCh > 0 ? 1.0f / static_cast<float>(nCh) : 0.0f;
        for (int i = 0; i < buf.numSamples; ++i) {
            float m = 0.0f;
            for (int ch = 0; ch < nCh; ++ch) m += buf.channels[ch][i];
            m *= norm;
            ring_[size_t(writePos_)] = std::isfinite(m) ? m : 0.0f;
            writePos_ = (writePos_ + 1) & (kFftSize - 1);
            if (++hopCount_ >= kHop) { hopCount_ = 0; analyse(); }
        }
    }

private:
    struct Slot {
        bool used = false;
        float hz = 0.0f, depthDb = 0.0f, targetDb = 0.0f, appliedDb = 1.0f;
        long long born = 0;
        Biquad bq;
    };
    struct Tracker { int bin = -1; int count = 0; float lastDb = -200.0f; long long seen = -10; };
    static constexpr float kQ = 30.0f;

    void updateFilters() {
        const float floorDb = maxDepthDb.get();
        const int limit = clampInt(static_cast<int>(std::lround(maxFilters.get())), 1, kMaxFilters);
        // Färre platser än använda filter: släpp de äldsta.
        while (usedCount() > limit) {
            Slot* oldest = nullptr;
            for (auto& s : slots_) if (s.used && (!oldest || s.born < oldest->born)) oldest = &s;
            if (oldest) *oldest = {};
        }
        bool changed = false;
        for (auto& s : slots_) {
            if (!s.used) continue;
            s.targetDb = std::max(s.targetDb, floorDb);
            // Glid mot måldjupet (1 dB per block) så att filtret inte klickar in.
            if (s.depthDb > s.targetDb) s.depthDb = std::max(s.targetDb, s.depthDb - 1.0f);
            else if (s.depthDb < s.targetDb) s.depthDb = std::min(s.targetDb, s.depthDb + 1.0f);
            if (s.depthDb != s.appliedDb) {
                s.bq.setCoeffs(BiquadCoeffs::peak(sr_, s.hz, kQ, s.depthDb));
                s.appliedDb = s.depthDb;
                changed = true;
            }
        }
        if (changed) publish();
    }

    static int clampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

    int usedCount() const { int n = 0; for (const auto& s : slots_) n += s.used ? 1 : 0; return n; }

    void publish() {
        int n = 0;
        for (size_t i = 0; i < slots_.size(); ++i) {
            hzOut_[i].store(slots_[i].used ? slots_[i].hz : 0.0f, std::memory_order_relaxed);
            depthOut_[i].store(slots_[i].used ? slots_[i].depthDb : 0.0f, std::memory_order_relaxed);
            n += slots_[i].used ? 1 : 0;
        }
        active_.store(n, std::memory_order_relaxed);
    }

    float at(int k) const { return k >= 0 && k <= kFftSize / 2 ? magDb_[size_t(k)] : -200.0f; }
    float maxAround(int k) const { return std::max({ at(k - 1), at(k), at(k + 1) }); }

    void analyse() {
        ++frameNo_;
        for (int i = 0; i < kFftSize; ++i) {
            const float x = ring_[size_t((writePos_ + i) & (kFftSize - 1))];
            frame_[size_t(i)] = { x * window_[size_t(i)], 0.0f };
        }
        fft_.forward(frame_.data());
        // dBFS: en sinus med amplitud A ger toppen A·N/4 med Hann-fönster.
        const float ref = 20.0f * std::log10(static_cast<float>(kFftSize) * 0.25f);
        for (int k = 0; k <= kFftSize / 2; ++k) {
            const auto c = frame_[size_t(k)];
            const float pw = c.real() * c.real() + c.imag() * c.imag() + 1e-20f;
            magDb_[size_t(k)] = 10.0f * std::log10(pw) - ref;
            prefix_[size_t(k) + 1] = prefix_[size_t(k)] + static_cast<double>(pw);   // summa för omgivningen
        }
        const double refPow = std::pow(10.0, ref * 0.1);

        const float s = sensitivity.get();
        const float paprThr = 26.0f - 14.0f * s;      // dB över omgivningen
        const float harmThr = 20.0f - 8.0f * s;       // dB över över-/undertoner
        const float floorDb = -38.0f - 14.0f * s;     // lägsta nivå som räknas
        const int persist = static_cast<int>(std::lround(16.0f - 10.0f * s));   // ≈ 340 … 130 ms

        const double binHz = sr_ / kFftSize;
        const int kLo = std::max(4, static_cast<int>(std::ceil(70.0 / binHz)));
        const int kHi = std::min(kFftSize / 2 - 3, static_cast<int>(12000.0 / binHz));
        for (int k = kLo; k <= kHi; ++k) {
            const float m = magDb_[size_t(k)];
            if (m < floorDb || m <= at(k - 1) || m < at(k + 1) || m < at(k - 2) || m < at(k + 2)) continue;
            // Topp mot omgivningen (effektmedel utanför ±3 bin, inom ±40).
            const int lo = std::max(1, k - 40), hi = std::min(kFftSize / 2, k + 40);
            const double sum = (prefix_[size_t(hi) + 1] - prefix_[size_t(lo)]) - (prefix_[size_t(k) + 4] - prefix_[size_t(k) - 3]);
            const int cnt = (hi - lo + 1) - 7;
            const float around = cnt > 0 ? 10.0f * static_cast<float>(std::log10(sum / cnt / refPow + 1e-30)) : -200.0f;
            if (m - around < paprThr) continue;
            // Övertoner (2f, 3f) och om toppen själv är en överton (f/2, f/3).
            const float harm = std::max({ maxAround(2 * k), maxAround(3 * k), maxAround(k / 2), maxAround(k / 3) });
            if (m - harm < harmThr) continue;
            track(k, m, persist);
        }
        // Spårare som inte sågs i det här fönstret släpps.
        for (auto& t : trackers_) if (t.bin >= 0 && t.seen != frameNo_) t = {};
    }

    void track(int k, float db, int persist) {
        Tracker* t = nullptr;
        for (auto& c : trackers_) if (c.bin >= 0 && std::abs(c.bin - k) <= 2 && c.seen == frameNo_ - 1) { t = &c; break; }
        if (!t) {
            for (auto& c : trackers_) if (c.bin < 0) { t = &c; break; }
            if (!t) return;
            *t = { k, 1, db, frameNo_ };
            return;
        }
        // Rundgång klingar inte av: nivån ska ligga still eller öka.
        t->count = db >= t->lastDb - 0.3f ? t->count + 1 : 0;
        t->lastDb = db;
        t->bin = k;
        t->seen = frameNo_;
        if (t->count >= persist) {
            t->count = 0;
            const float a = at(k - 1), b = at(k), c = at(k + 1);
            const float den = a - 2.0f * b + c;
            const float p = std::fabs(den) > 1e-6f ? std::clamp(0.5f * (a - c) / den, -0.5f, 0.5f) : 0.0f;
            trigger(static_cast<float>((k + p) * sr_ / kFftSize));
        }
    }

    void trigger(float hz) {
        const float floorDb = maxDepthDb.get();
        // Finns redan ett filter nära (inom 1/24 oktav): fördjupa det.
        for (auto& s : slots_) {
            if (s.used && std::fabs(std::log2(hz / s.hz)) < 1.0f / 24.0f) {
                s.targetDb = std::max(floorDb, s.targetDb - 3.0f);
                return;
            }
        }
        const int limit = clampInt(static_cast<int>(std::lround(maxFilters.get())), 1, kMaxFilters);
        Slot* target = nullptr;
        if (usedCount() < limit) { for (auto& s : slots_) if (!s.used) { target = &s; break; } }
        else { for (auto& s : slots_) if (s.used && (!target || s.born < target->born)) target = &s; }
        if (!target) return;
        target->bq.reset();
        target->used = true;
        target->hz = hz;
        target->depthDb = 0.0f;
        target->appliedDb = 1.0f;
        target->targetDb = std::max(floorDb, -9.0f);
        target->born = frameNo_;
    }

    double sr_ = 48000.0;
    int numCh_ = 2;
    FFT fft_;
    std::vector<float> ring_, magDb_, window_;
    std::vector<double> prefix_;
    std::vector<FFT::cfloat> frame_;
    int writePos_ = 0, hopCount_ = 0;
    long long frameNo_ = 0;
    std::array<Slot, kMaxFilters> slots_ {};
    std::array<Tracker, 16> trackers_ {};
    std::atomic<bool> resetRequested_ { false };
    std::atomic<int> active_ { 0 };
    std::array<std::atomic<float>, kMaxFilters> hzOut_ {}, depthOut_ {};
};

} // namespace gfx
