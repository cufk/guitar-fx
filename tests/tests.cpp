// Tester för motorn. Inga beroenden – kör med `ctest` eller ./gfx-tests.
//
// Det viktigaste testet för live-säkerhet: ljudtråden får aldrig allokera
// minne. Vi räknar alla anrop till operator new medan process() körs.

#include "engine/Engine.h"
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#include <new>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Allokeringsräknare
// ---------------------------------------------------------------------------
static std::atomic<bool> gCountAllocs { false };
static std::atomic<int>  gAllocCount { 0 };

void* operator new(std::size_t n) {
    if (gCountAllocs.load()) gAllocCount.fetch_add(1);
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

struct NoAllocScope {
    NoAllocScope()  { gAllocCount = 0; gCountAllocs = true; }
    ~NoAllocScope() { gCountAllocs = false; }
    int count() const { return gAllocCount.load(); }
};

// ---------------------------------------------------------------------------
// Mini-testramverk
// ---------------------------------------------------------------------------
static int gFailures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("  FEL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++gFailures; } } while (0)

static void run(const char* name, const std::function<void()>& fn) {
    const int before = gFailures;
    fn();
    std::printf("%s %s\n", gFailures == before ? "[ OK ]" : "[FAIL]", name);
}

// ---------------------------------------------------------------------------
// Hjälpare och test-effekter
// ---------------------------------------------------------------------------
constexpr double kSr = 48000.0;
constexpr int kBlock = 64;

struct Stereo {
    std::vector<float> l, r;
    float* p[2];
    explicit Stereo(int n) : l(static_cast<size_t>(n)), r(static_cast<size_t>(n)) { p[0] = l.data(); p[1] = r.data(); }
    gfx::AudioBufferView view(int start, int len) { static float* q[2]; q[0] = p[0] + start; q[1] = p[1] + start; return { q, 2, len }; }
};

static void sine(std::vector<float>& v, float amp, float hz) {
    for (size_t i = 0; i < v.size(); ++i)
        v[i] = amp * std::sin(2.0f * 3.14159265f * hz * static_cast<float>(i) / static_cast<float>(kSr));
}

static void runEngine(gfx::Engine& e, Stereo& s, int n, int block = kBlock) {
    for (int start = 0; start < n; start += block)
        e.process(s.view(start, std::min(block, n - start)));
}

struct BoostEffect final : gfx::Effect {
    float gain = 100.0f; // +40 dB
    std::string_view name() const override { return "Boost"; }
    void prepare(const gfx::ProcessSpec&) override {}
    void reset() override {}
    void process(gfx::AudioBufferView b) override {
        for (int ch = 0; ch < b.numChannels; ++ch)
            for (int i = 0; i < b.numSamples; ++i) b.channels[ch][i] *= gain;
    }
};

struct NaNEffect final : gfx::Effect {
    std::atomic<bool> fire { false };
    int resets = 0;
    std::string_view name() const override { return "NaN"; }
    void prepare(const gfx::ProcessSpec&) override {}
    void reset() override { ++resets; }
    void process(gfx::AudioBufferView b) override {
        if (fire.exchange(false))
            b.channels[0][b.numSamples / 2] = std::numeric_limits<float>::quiet_NaN();
    }
};

struct InvertEffect final : gfx::Effect {
    std::string_view name() const override { return "Invert"; }
    void prepare(const gfx::ProcessSpec&) override {}
    void reset() override {}
    void process(gfx::AudioBufferView b) override {
        for (int ch = 0; ch < b.numChannels; ++ch)
            for (int i = 0; i < b.numSamples; ++i) b.channels[ch][i] = -b.channels[ch][i];
    }
};

// ---------------------------------------------------------------------------
// Tester
// ---------------------------------------------------------------------------
int main() {
    run("Passthrough: tom kedja ger identisk signal (under limiter-taket)", [] {
        gfx::Engine e;
        e.prepare({ kSr, kBlock, 2 });
        const int n = 4800;
        Stereo s(n);
        sine(s.l, 0.5f, 220.0f);
        const auto ref = s.l;
        runEngine(e, s, n);
        float maxErr = 0.0f;
        for (int i = 0; i < n; ++i) {
            maxErr = std::max(maxErr, std::fabs(s.l[size_t(i)] - ref[size_t(i)]));
            maxErr = std::max(maxErr, std::fabs(s.r[size_t(i)] - ref[size_t(i)])); // dual mono
        }
        CHECK(maxErr < 1.0e-6f);
    });

    run("Limiter: utsignalen överstiger aldrig taket, även med +40 dB boost", [] {
        gfx::Engine e;
        e.chain().add(std::make_unique<BoostEffect>());
        e.prepare({ kSr, kBlock, 2 });
        const int n = 48000;
        Stereo s(n);
        sine(s.l, 0.9f, 110.0f);
        runEngine(e, s, n);
        const float ceiling = gfx::dbToGain(e.limiter().ceilingDb.get());
        float peak = 0.0f;
        for (int i = 0; i < n; ++i) peak = std::max({ peak, std::fabs(s.l[size_t(i)]), std::fabs(s.r[size_t(i)]) });
        CHECK(peak <= ceiling + 1.0e-6f);
        CHECK(e.limiterGainReductionDb() < -20.0f);
    });

    run("NaN-skydd: NaN tystas, kedjan återställs och ljudet fortsätter", [] {
        gfx::Engine e;
        auto nanFx = std::make_unique<NaNEffect>();
        auto* nan = nanFx.get();
        e.chain().add(std::move(nanFx));
        e.prepare({ kSr, kBlock, 2 });
        const int n = 4800;
        Stereo s(n);
        sine(s.l, 0.5f, 220.0f);
        const int resetsBefore = nan->resets;
        nan->fire = true;
        runEngine(e, s, n);
        bool allFinite = true;
        for (int i = 0; i < n; ++i) allFinite &= std::isfinite(s.l[size_t(i)]) && std::isfinite(s.r[size_t(i)]);
        CHECK(allFinite);
        CHECK(e.recoveryCount() == 1);
        CHECK(nan->resets > resetsBefore);
        CHECK(std::fabs(s.l[size_t(n - 10)]) > 0.0f || std::fabs(s.l[size_t(n - 20)]) > 0.0f); // ljud efteråt
    });

    run("Bypass: av/på glider mjukt utan hopp i signalen", [] {
        gfx::Engine e;
        const int slot = e.chain().add(std::make_unique<InvertEffect>(), false);
        e.prepare({ kSr, kBlock, 2 });
        const int n = 9600;
        Stereo s(n);
        std::fill(s.l.begin(), s.l.end(), 0.5f); // DC gör hopp tydliga: +0.5 -> -0.5
        for (int start = 0; start < n; start += kBlock) {
            if (start == kBlock * 37) e.chain().setEnabled(slot, true);
            e.process(s.view(start, kBlock));
        }
        float maxStep = 0.0f;
        for (int i = 1; i < n; ++i) maxStep = std::max(maxStep, std::fabs(s.l[size_t(i)] - s.l[size_t(i - 1)]));
        CHECK(maxStep < 0.01f);                       // inget klick (hårt byte skulle ge 1.0)
        CHECK(std::fabs(s.l[size_t(n - 1)] + 0.5f) < 1.0e-4f); // helt på efteråt
    });

    run("Stora block från värden delas upp (värd skickar 1024, maxBlockSize 256)", [] {
        gfx::Engine e;
        e.prepare({ kSr, 256, 2 });
        const int n = 4096;
        Stereo s(n);
        sine(s.l, 0.5f, 220.0f);
        const auto ref = s.l;
        runEngine(e, s, n, 1024);
        float maxErr = 0.0f;
        for (int i = 0; i < n; ++i) maxErr = std::max(maxErr, std::fabs(s.l[size_t(i)] - ref[size_t(i)]));
        CHECK(maxErr < 1.0e-6f);
    });

    run("Realtidssäkerhet: process() allokerar inget minne", [] {
        gfx::Engine e;
        e.chain().add(std::make_unique<BoostEffect>());
        const int slot = e.chain().add(std::make_unique<InvertEffect>(), false);
        e.prepare({ kSr, kBlock, 2 });
        Stereo s(kBlock * 200);
        sine(s.l, 0.5f, 220.0f);
        NoAllocScope scope;
        for (int b = 0; b < 200; ++b) {
            if (b == 50) e.chain().setEnabled(slot, true);   // crossfade-vägen
            if (b == 120) e.inputGain().gainDb.set(6.0f);    // smoothing-vägen
            e.process(s.view(b * kBlock, kBlock));
        }
        CHECK(scope.count() == 0);
    });

    std::printf("\n%s (%d fel)\n", gFailures == 0 ? "ALLA TESTER OK" : "TESTER MISSLYCKADES", gFailures);
    return gFailures == 0 ? 0 : 1;
}
