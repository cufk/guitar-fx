// Tester för motorn. Inga beroenden – kör med `ctest` eller ./gfx-tests.
//
// Det viktigaste testet för live-säkerhet: ljudtråden får aldrig allokera
// minne. Vi räknar alla anrop till operator new medan process() körs.

#include "engine/Engine.h"
#include "dsp/core/Convolver.h"
#include "dsp/core/Oversampler.h"
#include "dsp/effects/Cabinet.h"
#include "dsp/effects/Chorus.h"
#include "dsp/effects/Eq.h"
#include "engine/Rig.h"
#include "dsp/effects/Flanger.h"
#include "dsp/effects/Reverb.h"
#include "dsp/effects/Distortion.h"
#include <random>
#include <thread>
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


// Amplitud för en frekvens (Goertzel), över v[start, start+len).
static float toneAmplitude(const std::vector<float>& v, size_t start, size_t len, double hz) {
    const double w = 2.0 * 3.14159265358979 * hz / kSr, c = 2.0 * std::cos(w);
    double s1 = 0.0, s2 = 0.0;
    for (size_t i = start; i < start + len; ++i) { const double s0 = v[i] + c * s1 - s2; s2 = s1; s1 = s0; }
    const double re = s1 - s2 * std::cos(w), im = s2 * std::sin(w);
    return static_cast<float>(2.0 * std::sqrt(re * re + im * im) / static_cast<double>(len));
}

static bool allFiniteAndBounded(const Stereo& s, float bound) {
    for (size_t i = 0; i < s.l.size(); ++i)
        if (!std::isfinite(s.l[i]) || !std::isfinite(s.r[i]) || std::fabs(s.l[i]) > bound || std::fabs(s.r[i]) > bound) return false;
    return true;
}

// Kör en effekt direkt (utan motor) på hela bufferten i block.
static void runEffect(gfx::Effect& fx, Stereo& s, int block = kBlock) {
    const int n = static_cast<int>(s.l.size());
    for (int start = 0; start < n; start += block)
        fx.process(s.view(start, std::min(block, n - start)));
}

// Lägen och extremvärden som varje effekt ska klara.
static void stressInput(Stereo& s) {
    std::mt19937 rng(1);
    std::uniform_real_distribution<float> d(-1.0f, 1.0f);
    const size_t n = s.l.size();
    for (size_t i = 0; i < n; ++i) {
        float v = d(rng);
        if (i % 977 == 0) v = 1.0e6f;                 // jättespik
        if (i > n / 2 && i < n / 2 + 500) v = 1.0e-40f; // denormaler
        s.l[i] = s.r[i] = v;
    }
}

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


    // ----------------------------------------------------------------- Oversampler
    run("Oversampler: upp+ner utan waveshaper ger platt frekvensgång och rätt latens", [] {
        gfx::Oversampler4x os;
        os.prepare(64, 1);
        const int n = 48000;
        std::vector<float> out(static_cast<size_t>(n)), tmp(64 * 4);
        auto pass = [&](const std::vector<float>& in) {
            os.reset();
            for (int start = 0; start < n; start += 64) {
                os.up(0, in.data() + start, 64, tmp.data());
                os.down(0, tmp.data(), 64, out.data() + start);
            }
        };
        // Impuls: toppen ska ligga vid rapporterad latens (±1).
        std::vector<float> imp(static_cast<size_t>(n), 0.0f);
        imp[100] = 1.0f;
        pass(imp);
        int peakIdx = 0;
        for (int i = 0; i < n; ++i) if (std::fabs(out[size_t(i)]) > std::fabs(out[size_t(peakIdx)])) peakIdx = i;
        CHECK(std::abs((peakIdx - 100) - gfx::Oversampler4x::latencySamples()) <= 1);

        for (double hz : { 100.0, 1000.0, 8000.0, 16000.0, 19000.0 }) {
            std::vector<float> in(static_cast<size_t>(n));
            for (int i = 0; i < n; ++i) in[size_t(i)] = 0.5f * std::sin(float(2.0 * 3.14159265358979 * hz * i / kSr));
            pass(in);
            const float a = toneAmplitude(out, 4800, 38400, hz);
            CHECK(std::fabs(gfx::gainToDb(a / 0.5f)) < 0.2f);
        }
    });

    // ----------------------------------------------------------------- Distortion
    run("Distortion: tystnad in -> tystnad ut (alla lägen)", [] {
        for (int m = 0; m < 3; ++m) {
            gfx::Distortion d;
            d.mode.set(float(m));
            d.driveDb.set(48.0f);
            d.prepare({ kSr, kBlock, 2 });
            Stereo s(9600);
            runEffect(d, s);
            float peak = 0.0f;
            for (size_t i = 4800; i < s.l.size(); ++i) peak = std::max(peak, std::fabs(s.l[i]));
            CHECK(peak < 1.0e-6f);
        }
    });

    run("Distortion: extrema parametrar och indata ger inga NaN och begränsad nivå", [] {
        for (int m = 0; m < 3; ++m)
            for (float drive : { 0.0f, 48.0f })
                for (float tone : { 0.0f, 1.0f }) {
                    gfx::Distortion d;
                    d.mode.set(float(m)); d.driveDb.set(drive); d.tone.set(tone); d.levelDb.set(6.0f);
                    d.tightHz.set(400.0f);
                    d.prepare({ kSr, kBlock, 2 });
                    Stereo s(24000);
                    stressInput(s);
                    runEffect(d, s, 61); // udda blockstorlek
                    CHECK(allFiniteAndBounded(s, 8.0f));
                }
    });

    run("Distortion: waveshapers är begränsade och kontinuerliga", [] {
        for (int m = 0; m < 3; ++m) {
            float prev = gfx::Distortion::shapeSample(m, -10.0f), maxJump = 0.0f, maxAbs = 0.0f;
            for (float x = -10.0f; x <= 10.0f; x += 0.001f) {
                const float y = gfx::Distortion::shapeSample(m, x);
                maxAbs = std::max(maxAbs, std::fabs(y));
                maxJump = std::max(maxJump, std::fabs(y - prev));
                prev = y;
            }
            CHECK(maxAbs <= 1.0f);
            CHECK(maxJump < 0.01f);
            CHECK(std::isfinite(gfx::Distortion::shapeSample(m, std::numeric_limits<float>::quiet_NaN())));
            CHECK(std::fabs(gfx::Distortion::shapeSample(m, 0.0f)) < 1.0e-6f); // ingen DC i vila
        }
    });

    run("Distortion: oversampling håller aliasing under -70 dB (7 kHz, drive 24 dB)", [] {
        // Udda övertoner av 7 kHz: 35 kHz viks till 13 kHz och 49 kHz till 1 kHz
        // om de inte filtreras bort. Mät dessa relativt grundtonen.
        gfx::Distortion d;
        d.driveDb.set(24.0f); d.tone.set(1.0f); d.levelDb.set(0.0f); d.tightHz.set(20.0f);
        d.prepare({ kSr, kBlock, 2 });
        const int n = 48000;
        Stereo s(n);
        sine(s.l, 0.5f, 7000.0f);
        s.r = s.l;
        runEffect(d, s);
        const float fund = toneAmplitude(s.l, 9600, 38400, 7000.0);
        const float a13 = toneAmplitude(s.l, 9600, 38400, 13000.0);
        const float a1  = toneAmplitude(s.l, 9600, 38400, 1000.0);
        const float worst = gfx::gainToDb(std::max(a13, a1) / fund);
        std::printf("       aliasing: %.1f dB relativt grundton\n", worst);
        CHECK(worst < -70.0f);
    });

    run("Distortion: process() allokerar inget minne", [] {
        gfx::Distortion d;
        d.prepare({ kSr, kBlock, 2 });
        Stereo s(kBlock * 100);
        sine(s.l, 0.5f, 220.0f);
        NoAllocScope scope;
        for (int b = 0; b < 100; ++b) {
            if (b == 30) { d.tone.set(0.9f); d.driveDb.set(40.0f); d.mode.set(2.0f); d.tightHz.set(300.0f); }
            if (b == 60) d.reset();
            d.process(s.view(b * kBlock, kBlock));
        }
        CHECK(scope.count() == 0);
    });

    // ----------------------------------------------------------------- Faltning / kabinett
    run("Faltning: identisk med direkt faltning (IR 1000, udda block)", [] {
        std::mt19937 rng(7);
        std::uniform_real_distribution<float> d(-1.0f, 1.0f);
        std::vector<float> ir(1000), x(6000), ref(6000, 0.0f);
        for (auto& v : ir) v = d(rng) * 0.05f;
        for (auto& v : x) v = d(rng);
        for (size_t n = 0; n < x.size(); ++n)
            for (size_t k = 0; k < ir.size() && k <= n; ++k) ref[n] += ir[k] * x[n - k];

        gfx::FFT fft;
        fft.prepare(gfx::ConvolverIR::kFftSize);
        std::unique_ptr<gfx::ConvolverIR> c(gfx::ConvolverIR::build(ir.data(), int(ir.size()), fft));
        gfx::Convolver conv;
        conv.prepare(64);
        auto y = x;
        for (size_t start = 0; start < y.size(); start += 37)
            conv.process(c.get(), fft, y.data() + start, int(std::min<size_t>(37, y.size() - start)));
        float maxErr = 0.0f;
        for (size_t i = 0; i < y.size(); ++i) maxErr = std::max(maxErr, std::fabs(y[i] - ref[i]));
        CHECK(maxErr < 1.0e-4f);
    });

    run("Kabinett: tystnad, extrema indata och IR-byte under drift", [] {
        gfx::Cabinet cab;
        cab.levelDb.set(12.0f);
        cab.prepare({ kSr, kBlock, 2 });
        Stereo silent(9600);
        runEffect(cab, silent);
        float peak = 0.0f;
        for (float v : silent.l) peak = std::max(peak, std::fabs(v));
        CHECK(peak == 0.0f);

        Stereo s(48000);
        stressInput(s);
        runEffect(cab, s);
        CHECK(allFiniteAndBounded(s, 1.0e9f));

        // Byt till en IR som bara är en fördröjd impuls vid 44,1 kHz och kolla att den tar.
        std::vector<float> imp(441, 0.0f);
        imp[0] = 1.0f;
        CHECK(cab.loadImpulseResponse(imp.data(), int(imp.size()), 44100.0, "test"));
        CHECK(cab.impulseResponseName() == "test");
        cab.levelDb.set(0.0f);
        cab.reset();
        Stereo t(4800);
        sine(t.l, 0.5f, 440.0f);
        t.r = t.l;
        const auto ref = t.l;
        runEffect(cab, t);
        float err = 0.0f;
        for (size_t i = 2400; i < t.l.size(); ++i) err = std::max(err, std::fabs(t.l[i] - ref[i]));
        CHECK(err < 0.02f); // ≈ genomgång (liten avvikelse från omsampling)
        cab.collectGarbage();
    });

    run("Kabinett: IR-laddning från annan tråd medan ljudet går (inga allokeringar på ljudtråden)", [] {
        gfx::Cabinet cab;
        cab.prepare({ kSr, kBlock, 2 });
        Stereo s(kBlock * 2000);
        sine(s.l, 0.5f, 220.0f);
        std::atomic<bool> done { false };
        std::vector<float> ir = gfx::Cabinet::builtInImpulseResponse();
        std::thread loader([&] {
            for (int i = 0; i < 20; ++i) {
                ir[0] *= -1.0f;
                cab.loadImpulseResponse(ir.data(), int(ir.size()), 48000.0, "ir" + std::to_string(i));
                cab.collectGarbage();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            done = true;
        });
        // Allokeringar sker på loader-tråden, så räkna bara antalet på ljudtråden
        // genom att köra med räknaren avstängd och istället kontrollera att
        // process() är ren i ett separat pass nedan.
        for (int b = 0; b < 2000; ++b) cab.process(s.view(b * kBlock, kBlock));
        loader.join();
        CHECK(done.load());
        CHECK(allFiniteAndBounded(s, 100.0f));

        cab.loadImpulseResponse(ir.data(), int(ir.size()), 48000.0, "sista"); // väntar på upplockning
        NoAllocScope scope;
        for (int b = 0; b < 200; ++b) {
            if (b == 100) cab.reset();
            cab.process(s.view(b * kBlock, kBlock));
        }
        CHECK(scope.count() == 0);
    });

    // ----------------------------------------------------------------- Hela kedjan
    run("Kedja: in -> distortion -> kabinett med av/på och allt realtidssäkert", [] {
        gfx::Engine e;
        const int dist = e.chain().add(std::make_unique<gfx::Distortion>());
        const int cab  = e.chain().add(std::make_unique<gfx::Cabinet>(), false);
        e.prepare({ kSr, 256, 2 });
        CHECK(e.latencySamples() == gfx::Oversampler4x::latencySamples());
        Stereo s(kBlock * 400);
        sine(s.l, 0.4f, 196.0f);
        NoAllocScope scope;
        for (int b = 0; b < 400; ++b) {
            if (b == 50)  e.chain().setEnabled(cab, true);
            if (b == 150) e.chain().setEnabled(dist, false);
            if (b == 250) e.chain().setEnabled(dist, true);
            e.process(s.view(b * kBlock, kBlock));
        }
        CHECK(scope.count() == 0);
        CHECK(allFiniteAndBounded(s, 1.0f));
        CHECK(e.recoveryCount() == 0);
    });

    // ----------------------------------------------------------------- Latensmätning
    run("Latensmätning: simulerad loopback-kabel med 300 samples fördröjning", [] {
        gfx::Engine e;
        e.chain().add(std::make_unique<gfx::Distortion>()); // ska hoppas över under mätning
        e.prepare({ kSr, kBlock, 2 });
        constexpr int kDelay = 300;
        std::vector<float> line(kDelay + kBlock, 0.0f); // utsignal -> insignal
        std::vector<float> l(kBlock), r(kBlock);
        float* p[2] = { l.data(), r.data() };
        size_t wr = 0;
        auto& probe = e.latencyProbe();
        probe.start();
        // Lite brus och en svag lågpass "kabel/omvandlare".
        std::mt19937 rng(3);
        std::normal_distribution<float> noise(0.0f, 1.0e-4f);
        float lp = 0.0f;
        int blocks = 0;
        NoAllocScope scope;
        while (blocks < 10000 && (blocks < 2 || probe.isActive())) {
            for (int i = 0; i < kBlock; ++i) {
                const size_t rd = (wr + line.size() - kDelay + size_t(i)) % line.size();
                lp += 0.6f * (line[rd] - lp);
                l[size_t(i)] = lp + noise(rng);
                r[size_t(i)] = 0.0f;
            }
            e.process({ p, 2, kBlock });
            for (int i = 0; i < kBlock; ++i) line[(wr + size_t(i)) % line.size()] = l[size_t(i)];
            wr = (wr + kBlock) % line.size();
            ++blocks;
        }
        CHECK(scope.count() == 0);
        CHECK(probe.status() == gfx::LatencyProbe::Status::Done);
        CHECK(std::abs(probe.resultSamples() - kDelay) <= 1);
        std::printf("       uppmätt: %d samples (%.2f ms)\n", probe.resultSamples(), probe.resultMs());
    });

    run("Latensmätning: utan kabel rapporteras 'ingen signal'", [] {
        gfx::Engine e;
        e.prepare({ kSr, kBlock, 2 });
        Stereo s(kBlock);
        e.latencyProbe().start();
        for (int b = 0; b < 10000 && (b < 2 || e.latencyProbe().isActive()); ++b) {
            std::fill(s.l.begin(), s.l.end(), 0.0f);
            e.process(s.view(0, kBlock));
        }
        CHECK(e.latencyProbe().status() == gfx::LatencyProbe::Status::NoSignal);
    });


    // ----------------------------------------------------------------- Ingångssteg
    run("Ingång: källval 1/2 och mono-buss", [] {
        gfx::Engine e;
        e.prepare({ kSr, kBlock, 2 });
        const int n = 4800;
        Stereo s(n);
        sine(s.l, 0.3f, 220.0f);
        sine(s.r, 0.2f, 440.0f);
        const auto in1 = s.l, in2 = s.r;
        e.input().mode = gfx::InputMixer::Input2;
        e.input().reset(); // hoppa direkt till målvärdet (ingen glidning) för exakt jämförelse
        runEngine(e, s, n);
        float err = 0.0f;
        for (int i = 0; i < n; ++i) err = std::max({ err, std::fabs(s.l[size_t(i)] - in2[size_t(i)]), std::fabs(s.r[size_t(i)] - in2[size_t(i)]) });
        CHECK(err < 1.0e-6f);
        CHECK(std::fabs(e.inputPeak(0) - 0.3f) < 0.01f);
        CHECK(std::fabs(e.inputPeak(1) - 0.2f) < 0.01f);

        // Mono-buss: ingång 2 saknas -> ingång 1 används oavsett läge.
        gfx::Engine m;
        m.prepare({ kSr, kBlock, 1 });
        m.input().mode = gfx::InputMixer::Blend;
        std::vector<float> mono(in1);
        float* p[1];
        for (int start = 0; start < n; start += kBlock) {
            p[0] = mono.data() + start;
            m.process({ p, 1, std::min(kBlock, n - start) });
        }
        float merr = 0.0f;
        for (int i = 4800 / 2; i < n; ++i) merr = std::max(merr, std::fabs(mono[size_t(i)] - in1[size_t(i)]));
        CHECK(merr < 1.0e-5f);
    });

    run("Ingång: byte av källa och polaritet glider utan klick", [] {
        gfx::Engine e;
        e.prepare({ kSr, kBlock, 2 });
        const int n = 19200;
        Stereo s(n);
        std::fill(s.l.begin(), s.l.end(), 0.4f);  // DC gör hopp tydliga
        std::fill(s.r.begin(), s.r.end(), -0.3f);
        for (int start = 0, b = 0; start < n; start += kBlock, ++b) {
            if (b == 40)  e.input().mode = gfx::InputMixer::Input2;
            if (b == 100) e.input().mode = gfx::InputMixer::Blend;
            if (b == 160) e.input().invert2 = true;
            if (b == 220) e.input().alignMs.set(3.0f);
            e.process(s.view(start, kBlock));
        }
        float maxStep = 0.0f;
        for (int i = 1; i < n; ++i) maxStep = std::max(maxStep, std::fabs(s.l[size_t(i)] - s.l[size_t(i - 1)]));
        CHECK(maxStep < 0.01f);
        // Blanda i mitten, ingång 2 vänd: 0.4*0.707 + 0.3*0.707
        CHECK(std::fabs(s.l[size_t(n - 1)] - 0.7f * 0.70710678f) < 1.0e-3f);
    });

    run("Ingång: analys hittar fördröjning (bråkdel) och omvänd polaritet", [] {
        // Simulerad gitarr: filtrerat brus. Ingång 2 = -0.5 * ingång 1, 37,3 samples senare + brus.
        std::mt19937 rng(11);
        std::normal_distribution<float> nd(0.0f, 1.0f);
        const int n = 48000;
        std::vector<float> src(static_cast<size_t>(n + 100)), a(static_cast<size_t>(n)), b(static_cast<size_t>(n));
        float lp = 0.0f;
        for (auto& v : src) { lp += 0.3f * (nd(rng) - lp); v = lp; }
        const double lag = 37.3;
        for (int i = 0; i < n; ++i) {
            a[size_t(i)] = src[size_t(i + 50)];
            const double t = i + 50 - lag;
            const int k = int(std::floor(t));
            const float f = float(t - k);
            b[size_t(i)] = -0.5f * (src[size_t(k)] + f * (src[size_t(k + 1)] - src[size_t(k)])) + 0.01f * nd(rng);
        }
        const auto r = gfx::analyseAlignment(a.data(), b.data(), n, 240);
        std::printf("       hittade %.2f samples, %s, korrelation %.2f\n", r.lagSamples, r.invert ? "omvänd" : "normal", r.correlation);
        CHECK(std::fabs(r.lagSamples - lag) < 0.5);
        CHECK(r.invert);
        CHECK(r.correlation > 0.8);

        // Okorrelerade signaler ger låg korrelation.
        std::vector<float> c(static_cast<size_t>(n));
        for (auto& v : c) v = nd(rng);
        CHECK(gfx::analyseAlignment(a.data(), c.data(), n, 240).correlation < 0.2);
    });

    run("Ingång: automatisk justering i motorn ger koherent summa", [] {
        // Mikrofon (ingång 1) kommer 20 samples efter den inbyggda (ingång 2) och har omvänd polaritet.
        gfx::Engine e;
        e.prepare({ kSr, kBlock, 2 });
        e.input().mode = gfx::InputMixer::Blend;
        e.input().reset();
        std::mt19937 rng(5);
        std::normal_distribution<float> nd(0.0f, 0.1f);
        const int n = 48000 * 3, d = 20;
        std::vector<float> src(static_cast<size_t>(n + d));
        float lp = 0.0f;
        for (auto& v : src) { lp += 0.3f * (nd(rng) - lp); v = lp; }
        Stereo s(n);
        auto fill = [&] {
            for (int i = 0; i < n; ++i) { s.l[size_t(i)] = -src[size_t(i)]; s.r[size_t(i)] = src[size_t(i + d)]; }
        };
        auto rms = [&](size_t from) {
            double acc = 0.0;
            for (size_t i = from; i < s.l.size(); ++i) acc += double(s.l[i]) * s.l[i];
            return std::sqrt(acc / double(s.l.size() - from));
        };
        fill();
        e.input().requestCapture();
        runEngine(e, s, n);
        const double before = rms(size_t(n / 2)); // utan justering: nästan utsläckning
        const auto r = e.input().analyseCapture();
        CHECK(r.has_value());
        if (!r) return;
        CHECK(std::fabs(r->lagSamples + d) < 0.5); // ingång 2 är tidigare -> negativ lag
        CHECK(r->invert);
        e.input().apply(*r);
        CHECK(e.input().alignMs.get() > 0.0f); // fördröjer ingång 2
        fill();
        runEngine(e, s, n);
        const double after = rms(size_t(n / 2));
        double single = 0.0;
        for (size_t i = size_t(n / 2); i < size_t(n); ++i) single += double(src[i]) * src[i];
        single = std::sqrt(single / double(n - n / 2));
        // Perfekt koherent summa vid balans i mitten: 2 * 0.707 = 1.414 x en källa.
        const double ratio = after / single;
        std::printf("       före %.2f x, efter %.3f x en källa (ideal 1.414)\n", before / single, ratio);
        CHECK(std::fabs(ratio - 1.41421356) < 0.03);
        CHECK(after > before * 1.2);
    });

    run("Ingång: process() allokerar inget minne (inkl. inspelning för analys)", [] {
        gfx::Engine e;
        e.prepare({ kSr, kBlock, 2 });
        Stereo s(kBlock * 1000);
        sine(s.l, 0.3f, 220.0f);
        sine(s.r, 0.3f, 220.0f);
        e.input().requestCapture();
        NoAllocScope scope;
        for (int b = 0; b < 1000; ++b) {
            if (b == 100) e.input().mode = gfx::InputMixer::Blend;
            if (b == 200) e.input().alignMs.set(-4.0f);
            if (b == 300) e.input().invert1 = true;
            e.process(s.view(b * kBlock, kBlock));
        }
        CHECK(scope.count() == 0);
        CHECK(allFiniteAndBounded(s, 1.0f));
    });


    // ----------------------------------------------------------------- Chorus / Flanger / Reverb
    // Gemensamma krav för varje ny effekt: tystnad, extrema parametrar, mix 0 = genomgång, inga allokeringar.
    struct FxCase {
        const char* name;
        std::function<std::unique_ptr<gfx::Effect>()> make;
        std::function<void(gfx::Effect&, bool extreme)> setExtreme;
        std::function<void(gfx::Effect&)> setDry;
    };
    const std::vector<FxCase> fxCases = {
        { "EQ",
          [] { return std::make_unique<gfx::Eq>(); },
          [](gfx::Effect& e, bool hi) { auto& q = static_cast<gfx::Eq&>(e);
              for (auto* p : { &q.lowCutHz, &q.lowGainDb, &q.midFreqHz, &q.midGainDb, &q.highGainDb, &q.notchHz, &q.notchDepthDb })
                  p->set(hi ? p->max() : p->min()); },
          [](gfx::Effect& e) { auto& q = static_cast<gfx::Eq&>(e); q.lowCutHz.set(20.0f); } },
        { "Chorus",
          [] { return std::make_unique<gfx::Chorus>(); },
          [](gfx::Effect& e, bool hi) { auto& c = static_cast<gfx::Chorus&>(e);
              c.rateHz.set(hi ? 5.0f : 0.05f); c.depth.set(hi ? 1.0f : 0.0f); c.mix.set(hi ? 1.0f : 0.0f); },
          [](gfx::Effect& e) { static_cast<gfx::Chorus&>(e).mix.set(0.0f); } },
        { "Flanger",
          [] { return std::make_unique<gfx::Flanger>(); },
          [](gfx::Effect& e, bool hi) { auto& f = static_cast<gfx::Flanger&>(e);
              f.rateHz.set(hi ? 5.0f : 0.05f); f.depth.set(hi ? 1.0f : 0.0f);
              f.feedback.set(hi ? 0.95f : -0.95f); f.mix.set(1.0f); },
          [](gfx::Effect& e) { static_cast<gfx::Flanger&>(e).mix.set(0.0f); } },
        { "Reverb",
          [] { return std::make_unique<gfx::Reverb>(); },
          [](gfx::Effect& e, bool hi) { auto& r = static_cast<gfx::Reverb&>(e);
              r.decaySec.set(hi ? 10.0f : 0.3f); r.size.set(hi ? 1.0f : 0.0f); r.predelayMs.set(hi ? 120.0f : 0.0f);
              r.dampHz.set(hi ? 16000.0f : 1500.0f); r.lowCutHz.set(hi ? 20.0f : 600.0f); r.mix.set(1.0f); },
          [](gfx::Effect& e) { static_cast<gfx::Reverb&>(e).mix.set(0.0f); } },
    };

    for (const auto& fc : fxCases) {
        run((std::string(fc.name) + ": tystnad in -> tystnad ut").c_str(), [&] {
            for (bool hi : { false, true }) {
                auto fx = fc.make();
                fc.setExtreme(*fx, hi);
                fx->prepare({ kSr, kBlock, 2 });
                Stereo s(48000);
                runEffect(*fx, s);
                float peak = 0.0f;
                for (size_t i = 0; i < s.l.size(); ++i) peak = std::max({ peak, std::fabs(s.l[i]), std::fabs(s.r[i]) });
                CHECK(peak == 0.0f);
            }
        });

        run((std::string(fc.name) + ": extrema parametrar och indata ger inga NaN och begränsad nivå").c_str(), [&] {
            for (bool hi : { false, true }) {
                auto fx = fc.make();
                fc.setExtreme(*fx, hi);
                fx->prepare({ kSr, kBlock, 2 });
                Stereo s(96000);
                stressInput(s);
                runEffect(*fx, s, 61);
                // Spikar på 1e6 passerar torrt; poängen är att inget blir NaN/Inf eller växer.
                CHECK(allFiniteAndBounded(s, 1.0e7f));
                // Efter stressen: tystnad ska klinga av (ingen självsvängning).
                Stereo q(48000 * 12);
                runEffect(*fx, q);
                // Svansen ska avta med minst 40 dB på 12 s (10 s reverb ger 72 dB).
                float head = 0.0f, tail = 0.0f;
                for (size_t i = 0; i < 4800; ++i) head = std::max({ head, std::fabs(q.l[i]), std::fabs(q.r[i]) });
                for (size_t i = q.l.size() - 4800; i < q.l.size(); ++i) tail = std::max({ tail, std::fabs(q.l[i]), std::fabs(q.r[i]) });
                CHECK(tail < 1.0e-3f || tail < head * 0.01f);
            }
        });

        if (std::string(fc.name) != "EQ")
        run((std::string(fc.name) + ": mix 0 ger exakt genomgång").c_str(), [&] {
            auto fx = fc.make();
            fc.setDry(*fx);
            fx->prepare({ kSr, kBlock, 2 });
            Stereo s(9600);
            sine(s.l, 0.5f, 330.0f);
            s.r = s.l;
            const auto ref = s.l;
            runEffect(*fx, s);
            float err = 0.0f;
            for (size_t i = 0; i < s.l.size(); ++i) err = std::max({ err, std::fabs(s.l[i] - ref[i]), std::fabs(s.r[i] - ref[i]) });
            CHECK(err < 1.0e-6f);
        });

        run((std::string(fc.name) + ": process() allokerar inget minne").c_str(), [&] {
            auto fx = fc.make();
            fx->prepare({ kSr, kBlock, 2 });
            Stereo s(kBlock * 300);
            sine(s.l, 0.5f, 220.0f);
            s.r = s.l;
            NoAllocScope scope;
            for (int b = 0; b < 300; ++b) {
                if (b == 100) fc.setExtreme(*fx, true);
                if (b == 200) { fc.setExtreme(*fx, false); fx->reset(); }
                fx->process(s.view(b * kBlock, kBlock));
            }
            CHECK(scope.count() == 0);
        });
    }

    run("Chorus och flanger: vänster och höger skiljer sig (stereo)", [] {
        for (int which = 0; which < 2; ++which) {
            std::unique_ptr<gfx::Effect> fx;
            if (which == 0) fx = std::make_unique<gfx::Chorus>(); else fx = std::make_unique<gfx::Flanger>();
            fx->prepare({ kSr, kBlock, 2 });
            Stereo s(48000);
            sine(s.l, 0.5f, 440.0f);
            s.r = s.l;
            runEffect(*fx, s);
            float diff = 0.0f;
            for (size_t i = 24000; i < s.l.size(); ++i) diff = std::max(diff, std::fabs(s.l[i] - s.r[i]));
            CHECK(diff > 0.05f);
        }
    });

    run("Flanger: max återkoppling med fullskalig insignal förblir begränsad", [] {
        gfx::Flanger f;
        f.feedback.set(0.95f); f.mix.set(1.0f); f.depth.set(0.0f);
        f.prepare({ kSr, kBlock, 2 });
        Stereo s(48000 * 5);
        sine(s.l, 1.0f, 1000.0f);
        s.r = s.l;
        runEffect(f, s);
        float peak = 0.0f;
        for (float v : s.l) peak = std::max(peak, std::fabs(v));
        std::printf("       topp: %.2f\n", peak);
        CHECK(peak < 25.0f); // softClip i slingan: begränsad (limitern tar resten)
    });

    run("Reverb: efterklangstiden stämmer med inställningen (RT60 2 s, ±30 %)", [] {
        gfx::Reverb r;
        r.decaySec.set(2.0f); r.mix.set(1.0f); r.predelayMs.set(0.0f);
        r.dampHz.set(16000.0f); r.lowCutHz.set(20.0f);
        r.prepare({ kSr, kBlock, 2 });
        const int n = 48000 * 4;
        Stereo s(n);
        // Kort brusskur som excitation.
        std::mt19937 rng(2);
        std::uniform_real_distribution<float> d(-0.5f, 0.5f);
        for (int i = 0; i < 480; ++i) s.l[size_t(i)] = s.r[size_t(i)] = d(rng);
        runEffect(r, s);
        // Energi i fönster om 20 ms -> dB.
        const int win = 960;
        std::vector<double> db;
        for (int start = 4800; start + win <= n; start += win) {
            double e = 0.0;
            for (int i = start; i < start + win; ++i) e += double(s.l[size_t(i)]) * s.l[size_t(i)] + double(s.r[size_t(i)]) * s.r[size_t(i)];
            db.push_back(10.0 * std::log10(e + 1e-30));
        }
        // Linjär regression på -5..-35 dB under starten.
        const double ref = db[0];
        double sx = 0, sy = 0, sxx = 0, sxy = 0; int cnt = 0;
        for (size_t k = 0; k < db.size(); ++k) {
            const double rel = db[k] - ref;
            if (rel > -5.0 || rel < -35.0) continue;
            const double t = double(k) * win / kSr;
            sx += t; sy += db[k]; sxx += t * t; sxy += t * db[k]; ++cnt;
        }
        CHECK(cnt > 5);
        const double slope = (cnt * sxy - sx * sy) / (cnt * sxx - sx * sx); // dB/s
        const double rt60 = -60.0 / slope;
        std::printf("       uppmätt RT60: %.2f s\n", rt60);
        CHECK(rt60 > 1.4 && rt60 < 2.6);
    });

    run("Kedja: alla effekter på samtidigt, av/på, realtidssäkert", [] {
        gfx::Engine e;
        std::vector<int> slots;
        slots.push_back(e.chain().add(std::make_unique<gfx::Distortion>(), false));
        slots.push_back(e.chain().add(std::make_unique<gfx::Cabinet>(), false));
        slots.push_back(e.chain().add(std::make_unique<gfx::Chorus>()));
        slots.push_back(e.chain().add(std::make_unique<gfx::Flanger>()));
        slots.push_back(e.chain().add(std::make_unique<gfx::Reverb>()));
        e.prepare({ kSr, 256, 2 });
        Stereo s(kBlock * 600);
        sine(s.l, 0.4f, 196.0f);
        NoAllocScope scope;
        for (int b = 0; b < 600; ++b) {
            if (b % 50 == 0) { const int sl = slots[size_t((b / 50) % slots.size())]; e.chain().setEnabled(sl, !e.chain().isEnabled(sl)); }
            e.process(s.view(b * kBlock, kBlock));
        }
        CHECK(scope.count() == 0);
        CHECK(allFiniteAndBounded(s, 1.0f));
        CHECK(e.recoveryCount() == 0);
    });


    run("EQ: platt i mitten, notch dämpar sin frekvens, bas/diskant verkar", [] {
        auto gainAt = [](gfx::Eq& q, double hz) {
            q.prepare({ kSr, kBlock, 2 });
            Stereo s(48000);
            sine(s.l, 0.25f, float(hz));
            s.r = s.l;
            runEffect(q, s);
            return gfx::gainToDb(toneAmplitude(s.l, 24000, 19200, hz) / 0.25f);
        };
        gfx::Eq flat;
        CHECK(std::fabs(gainAt(flat, 1000.0)) < 0.3f);
        gfx::Eq notch;
        notch.notchHz.set(500.0f); notch.notchDepthDb.set(-24.0f);
        CHECK(gainAt(notch, 500.0) < -20.0f);
        CHECK(std::fabs(gainAt(notch, 2000.0)) < 0.5f);  // smal: påverkar inte långt bort
        gfx::Eq tilt;
        tilt.lowGainDb.set(9.0f); tilt.highGainDb.set(-9.0f); tilt.lowCutHz.set(20.0f);
        CHECK(gainAt(tilt, 60.0) > 6.0f);
        CHECK(gainAt(tilt, 12000.0) < -6.0f);
        gfx::Eq cut;
        cut.lowCutHz.set(300.0f);
        CHECK(gainAt(cut, 60.0) < -20.0f);
    });

    run("Reverb: svansen klingar ut när den stängs av, torr signal oförändrad", [] {
        gfx::Engine e;
        auto rv = std::make_unique<gfx::Reverb>();
        rv->decaySec.set(2.0f); rv->mix.set(0.5f);
        const int slot = e.chain().add(std::move(rv), true);
        e.prepare({ kSr, kBlock, 2 });
        // 1 s brus, sedan tystnad; reverben stängs av precis när insignalen tystnar.
        const int n = 48000 * 3;
        Stereo s(n);
        std::mt19937 rng(9);
        std::uniform_real_distribution<float> d(-0.3f, 0.3f);
        for (int i = 0; i < 48000; ++i) s.l[size_t(i)] = s.r[size_t(i)] = d(rng);
        for (int start = 0; start < n; start += kBlock) {
            if (start == 48000) e.chain().setEnabled(slot, false);
            e.process(s.view(start, kBlock));
        }
        auto peakIn = [&](int from, int to) { float p = 0; for (int i = from; i < to; ++i) p = std::max(p, std::fabs(s.l[size_t(i)])); return p; };
        const float tailEarly = peakIn(48000 + 2400, 48000 + 9600);   // 50–200 ms efter
        const float tailLate  = peakIn(48000 * 3 - 4800, 48000 * 3);
        std::printf("       svans 50–200 ms: %.3f, efter 2 s: %.5f\n", tailEarly, tailLate);
        CHECK(tailEarly > 0.01f);          // klipps inte
        CHECK(tailLate < tailEarly * 0.1f); // men klingar av

        // Torr signal medan reverben är av (och svansen är slut): exakt genomgång.
        Stereo silence(48000 * 3);
        runEngine(e, silence, 48000 * 3);
        Stereo t(9600);
        sine(t.l, 0.3f, 440.0f);
        const auto ref = t.l;
        runEngine(e, t, 9600);
        float err = 0.0f;
        for (size_t i = 0; i < ref.size(); ++i) err = std::max(err, std::fabs(t.l[i] - ref[i]));
        CHECK(err < 1.0e-5f);
    });

    run("Rig: preset tar över sina effekter, grundljudet styr resten", [] {
        gfx::Engine e;
        auto eq = std::make_unique<gfx::Eq>();
        auto rv = std::make_unique<gfx::Reverb>();
        auto ch = std::make_unique<gfx::Chorus>();
        auto* eqp = eq.get(); auto* rvp = rv.get(); auto* chp = ch.get();
        const int eqSlot = e.chain().add(std::move(eq), true);
        const int rvSlot = e.chain().add(std::move(rv), true);
        const int chSlot = e.chain().add(std::move(ch), false);
        gfx::Rig rig(e.chain());
        rig.addEffect({ "eq", "EQ", 0, eqSlot, { { "low", "Bas", "dB", &eqp->lowGainDb } } });
        rig.addEffect({ "reverb", "Reverb", 0, rvSlot, { { "mix", "Mix", "", &rvp->mix }, { "decay", "Efterklang", "s", &rvp->decaySec } } });
        rig.addEffect({ "chorus", "Chorus", 0, chSlot, { { "mix", "Mix", "", &chp->mix } } });
        eqp->lowGainDb.set(3.0f); rvp->mix.set(0.2f);
        rig.initBaseFromLive();

        // Preset "Hall": större reverb + chorus på.
        const int hall = rig.addPreset("Hall");
        rig.activate(hall);
        rig.setIncludedInActive("reverb", true);
        rig.setIncludedInActive("chorus", true);
        rvp->mix.set(0.5f); rvp->decaySec.set(5.0f); chp->mix.set(0.3f);
        rig.syncFromLive();
        CHECK(e.chain().isEnabled(chSlot));

        // Ändra EQ medan Hall är aktivt -> går till grundljudet (EQ ingår inte i Hall).
        eqp->lowGainDb.set(5.0f);
        rig.syncFromLive();
        CHECK(rig.base().at("eq").values.at("low") == 5.0f);

        // Tillbaka till grundljudet: reverb som innan, chorus av, EQ behåller ändringen.
        rig.select(hall); // samma igen => grundljud
        CHECK(rig.activePreset() == -1);
        CHECK(std::fabs(rvp->mix.get() - 0.2f) < 1e-6f);
        CHECK(std::fabs(rvp->decaySec.get() - 2.0f) < 1e-6f);
        CHECK(!e.chain().isEnabled(chSlot));
        CHECK(std::fabs(eqp->lowGainDb.get() - 5.0f) < 1e-6f);

        // Aktivera Hall igen: dess värden kommer tillbaka.
        rig.select(hall);
        CHECK(std::fabs(rvp->mix.get() - 0.5f) < 1e-6f);
        CHECK(std::fabs(rvp->decaySec.get() - 5.0f) < 1e-6f);
        CHECK(e.chain().isEnabled(chSlot));

        // Ta bort chorus ur presetet -> följer grundljudet (av).
        rig.setIncludedInActive("chorus", false);
        CHECK(!e.chain().isEnabled(chSlot));
        CHECK(!rig.activeIncludes("chorus"));
    });

    run("Rig: nästa/föregående, ta bort, flytta, duplicera, ladda", [] {
        gfx::Engine e;
        auto rv = std::make_unique<gfx::Reverb>();
        auto* rvp = rv.get();
        const int slot = e.chain().add(std::move(rv), false);
        gfx::Rig rig(e.chain());
        rig.addEffect({ "reverb", "Reverb", 0, slot, { { "mix", "Mix", "", &rvp->mix } } });
        rig.initBaseFromLive();
        for (const char* n : { "A", "B", "C" }) rig.addPreset(n);
        rig.selectNext(); CHECK(rig.activePreset() == 0);
        rig.selectNext(); rig.selectNext(); CHECK(rig.activePreset() == 2);
        rig.selectNext(); CHECK(rig.activePreset() == -1);
        rig.selectPrevious(); CHECK(rig.activePreset() == 2);

        rig.activate(1);
        rig.movePreset(1, 0);
        CHECK(rig.activePreset() == 0 && rig.activeName() == "B");
        const int dup = rig.duplicatePreset(0);
        CHECK(dup == 1 && rig.presets()[1].name == "B (kopia)");
        rig.removePreset(0);
        CHECK(rig.activePreset() == -1);
        CHECK(rig.presets().size() == 3);

        // Ladda: okänd effekt och okänt värde ignoreras, saknat värde fylls från grundljudet.
        gfx::Snapshot base { { "reverb", { true, { { "mix", 0.3f }, { "okand", 1.0f } } } } };
        std::vector<gfx::Preset> presets { { "Stor", { { "reverb", { true, { { "mix", 0.7f } } } }, { "finnsinte", {} } } } };
        rig.load(base, presets, 0);
        CHECK(rig.activePreset() == 0);
        CHECK(std::fabs(rvp->mix.get() - 0.7f) < 1e-6f);
        CHECK(e.chain().isEnabled(slot));
        CHECK(rig.presets()[0].overrides.size() == 1);
        rig.select(0);
        CHECK(std::fabs(rvp->mix.get() - 0.3f) < 1e-6f);
    });

    std::printf("\n%s (%d fel)\n", gFailures == 0 ? "ALLA TESTER OK" : "TESTER MISSLYCKADES", gFailures);
    return gFailures == 0 ? 0 : 1;
}
