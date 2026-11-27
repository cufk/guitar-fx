#include "ScreenSaver.h"
#include <cmath>

namespace ui {

namespace {
// Mjukt "brus" av överlagrade sinusvågor – billigt, jämnt och utan tabeller.
inline float n3(float x, float y, float z) {
    return std::sin(x * 1.7f + std::sin(y * 1.3f + z)) * 0.5f
         + std::sin(y * 2.1f + std::sin(z * 1.9f + x * 0.7f)) * 0.3f
         + std::sin(z * 2.7f + x * 1.1f) * 0.2f;
}
inline float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
float rnd() { return juce::Random::getSystemRandom().nextFloat(); }
} // namespace

const char* ScreenSaver::modeName(int m) {
    static const char* n[] = { "Ingen skärmsläckare", "Strömmar", "Klot", "Terräng", "Form" };
    return n[juce::jlimit(0, NumModes - 1, m)];
}
const char* ScreenSaver::shortName(int m) {
    static const char* n[] = { "Av", "Strömmar", "Klot", "Terräng", "Form" };
    return n[juce::jlimit(0, NumModes - 1, m)];
}

ScreenSaver::ScreenSaver(GuitarFXProcessor& proc) : proc_(proc) {
    setOpaque(true);
    setWantsKeyboardFocus(false);
    window_.resize(1024);
    // Klot: jämnt fördelade punkter på en sfär (Fibonacci-gitter).
    constexpr int N = 9000;
    const float g = juce::MathConstants<float>::pi * (3.0f - std::sqrt(5.0f));
    sphere_.reserve(N);
    for (int i = 0; i < N; ++i) {
        const float y = 1.0f - (static_cast<float>(i) / (N - 1)) * 2.0f, r = std::sqrt(1.0f - y * y), th = g * static_cast<float>(i);
        sphere_.push_back({ std::cos(th) * r, y, std::sin(th) * r, rnd() });
    }
    particles_.resize(26000);
    for (auto& p : particles_) p = { rnd(), rnd(), rnd(), static_cast<int>(rnd() * 300.0f) };
    fieldAx_.assign(kGW * kGH, 0.0f);
    fieldAy_.assign(kGW * kGH, 0.0f);
    fieldBody_.assign(kGW * kGH, 0.0f);
}

void ScreenSaver::start(int mode) {
    if (mode <= None || mode >= NumModes) return;
    mode_ = mode;
    running_ = true;
    startMs_ = juce::Time::getMillisecondCounterHiRes();
    std::fill(buf_.begin(), buf_.end(), 0.0f);
    setAlpha(0.0f);
    setVisible(true);
    toFront(false);
    juce::Desktop::getInstance().getAnimator().fadeIn(this, 800);
    startTimerHz(30);
}

void ScreenSaver::stop() {
    if (!running_) return;
    running_ = false;
    stopTimer();
    setVisible(false);
}

void ScreenSaver::resized() {
    bw_ = juce::jmax(1, getWidth());
    bh_ = juce::jmax(1, getHeight());
    buf_.assign(static_cast<size_t>(bw_ * bh_), 0.0f);
    image_ = juce::Image(juce::Image::RGB, bw_, bh_, true);
}

// ---------------------------------------------------------------------------
// Gitarren: nivå, bas, diskant och anslag ur de senaste ~21 ms av ingången.
void ScreenSaver::analyse() {
    proc_.engine().tuner().latest(window_.data(), static_cast<int>(window_.size()));
    double e = 0.0, lo = 0.0, hi = 0.0;
    float lp = 0.0f, lp2 = 0.0f;
    const float aLow = 0.026f, aHigh = 0.25f;   // ≈ 200 Hz och ≈ 2 kHz vid 48 kHz
    for (float x : window_) {
        lp += aLow * (x - lp);
        lp2 += aHigh * (x - lp2);
        e += static_cast<double>(x) * x;
        lo += static_cast<double>(lp) * lp;
        hi += static_cast<double>(x - lp2) * (x - lp2);
    }
    const float n = static_cast<float>(window_.size());
    auto toNorm = [](double energy, float count, float floorDb) {
        const float db = 10.0f * std::log10(static_cast<float>(energy) / count + 1e-12f);
        return clamp01((db - floorDb) / 36.0f);
    };
    const float L = toNorm(e, n, -60.0f), Lo = toNorm(lo, n, -62.0f), Hi = toNorm(hi, n, -70.0f);
    const float k = 0.15f;
    lvl_ += (L - lvl_) * k;
    low_ += (Lo - low_) * k;
    high_ += (Hi - high_) * k;
    // Anslag: nivån hoppar över ett långsamt medel.
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (L > slow_ + 0.12f && L > 0.25f && now - lastOnsetMs_ > 180.0) {
        lastOnsetMs_ = now;
        ++onsets_;
        waves_[static_cast<size_t>(nextWave_)] = { now, clamp01(L) };
        nextWave_ = (nextWave_ + 1) % static_cast<int>(waves_.size());
    }
    slow_ += (L - slow_) * 0.03f;
}

inline void ScreenSaver::dot(float x, float y, float a, int size) {
    const int ix = static_cast<int>(x), iy = static_cast<int>(y);
    if (ix < 0 || iy < 0 || ix + size > bw_ || iy + size > bh_) return;
    for (int dy = 0; dy < size; ++dy) {
        float* row = buf_.data() + (iy + dy) * bw_ + ix;
        for (int dx = 0; dx < size; ++dx) row[dx] += a;
    }
}

// ---------------------------------------------------------------------------
// Strömmar: prickar som flyter längs breda, oregelbundna floder (brant diagonal).
void ScreenSaver::computeFlowField(float t) {
    const float asp = static_cast<float>(bw_) / static_cast<float>(bh_), phi = 1.12f;
    const float dx = std::cos(phi), dy = std::sin(phi);
    for (int gy = 0; gy < kGH; ++gy) for (int gx = 0; gx < kGW; ++gx) {
        const float X = static_cast<float>(gx) / (kGW - 1) * asp, Y = static_cast<float>(gy) / (kGH - 1);
        const float u = X * dx + Y * dy, w = -X * dy + Y * dx;
        const float bend = 0.45f * n3(u * 1.6f, w * 1.2f, t * 0.05f) + 0.4f * high_ * std::sin(u * 8.0f + t * 2.0f);
        const size_t i = static_cast<size_t>(gy * kGW + gx);
        fieldAx_[i] = dx * std::cos(bend) - dy * std::sin(bend);
        fieldAy_[i] = dx * std::sin(bend) + dy * std::cos(bend);
        const float warp = 4.2f * n3(u * 0.7f, w * 0.55f, t * 0.03f) + 1.6f * n3(u * 1.9f + 5.0f, w * 1.6f, 1.7f + t * 0.05f);
        const float river = std::sin(w * 5.2f + warp);
        const float width = 1.0f + 1.6f * (0.5f + 0.5f * n3(u * 1.1f + 9.0f, w * 0.8f, 4.4f));
        const float body = std::max(0.0f, 1.0f - std::fabs(river) * width);
        const float cloud = 0.3f + 0.7f * (0.5f + 0.5f * n3(u * 1.3f + 2.0f, w * 1.1f, 3.1f + t * 0.02f));
        fieldBody_[i] = std::pow(body, 1.4f) * cloud * 1.1f;
    }
}

void ScreenSaver::renderStreams(float t) {
    for (auto& v : buf_) v *= 0.3f;                        // skarpa prickar, korta spår
    if (frame_ % 3 == 0) computeFlowField(t);              // fältet rör sig långsamt
    const float asp = static_cast<float>(bw_) / static_cast<float>(bh_);
    const float v = 0.0005f + 0.0018f * lvl_;
    const float bright = 0.6f + 0.4f * std::max(lvl_, 0.35f);
    for (auto& p : particles_) {
        const float fx = clamp01(p.x) * (kGW - 1), fy = clamp01(p.y) * (kGH - 1);
        const int gx = static_cast<int>(fx), gy = static_cast<int>(fy);
        const int gx1 = std::min(gx + 1, kGW - 1), gy1 = std::min(gy + 1, kGH - 1);
        const size_t i = static_cast<size_t>(gy * kGW + gx);
        p.x += fieldAx_[i] * v / asp;
        p.y += fieldAy_[i] * v;
        if (--p.life < 0 || p.x < -0.05f || p.x > 1.05f || p.y < -0.05f || p.y > 1.05f) {
            p.x = rnd() * 1.1f - 0.05f; p.y = rnd() * 1.1f - 0.05f; p.life = 150 + static_cast<int>(rnd() * 300.0f);
        }
        const float tx = fx - static_cast<float>(gx), ty = fy - static_cast<float>(gy);
        auto B = [&](int x, int y) { return fieldBody_[static_cast<size_t>(y * kGW + x)]; };
        const float body = (B(gx, gy) * (1 - tx) + B(gx1, gy) * tx) * (1 - ty) + (B(gx, gy1) * (1 - tx) + B(gx1, gy1) * tx) * ty;
        const float dust = p.s < 0.22f ? 0.16f : 0.0f;
        const float a = std::max(body, dust) * bright * (0.6f + 0.4f * p.s);
        if (a < 0.03f) continue;
        dot(p.x * static_cast<float>(bw_), p.y * static_cast<float>(bh_), a, p.s > 0.985f ? 3 : p.s > 0.9f ? 2 : 1);
    }
}

// ---------------------------------------------------------------------------
// Gemensam projektion för 3D-effekterna.
namespace {
struct Proj { float x, y, z; };
inline Proj project(float x, float y, float z, float cam, float w, float h) {
    const float zz = z + cam, f = std::min(w, h) * 0.9f / zz;
    return { w * 0.5f + x * f, h * 0.5f + y * f, zz };
}
} // namespace

// Klot: punktmoln som roterar och böljer – basen ger stora rörelser, diskanten skimmer.
void ScreenSaver::renderBlob(float t) {
    std::fill(buf_.begin(), buf_.end(), 0.0f);
    const float rotY = t * 0.18f, rotX = 0.35f + 0.15f * std::sin(t * 0.1f);
    const float cy = std::cos(rotY), sy = std::sin(rotY), cx = std::cos(rotX), sx = std::sin(rotX);
    const float big = 0.14f + 0.34f * low_, fine = 0.02f + 0.07f * high_;
    const float W = static_cast<float>(bw_), H = static_cast<float>(bh_);
    for (const auto& q : sphere_) {
        const float x = q[0], y = q[1], z = q[2], s = q[3];
        const float d = 1.0f + big * n3(x * 1.4f + t * 0.4f, y * 1.4f, z * 1.4f - t * 0.3f) + fine * std::sin(x * 12 + y * 9 + t * 3 + s * 6);
        const float X = x * d, Y = y * d, Z = z * d;
        const float X1 = X * cy + Z * sy, Z1 = -X * sy + Z * cy, Y1 = Y * cx - Z1 * sx, Z2 = Y * sx + Z1 * cx;
        const auto p = project(X1, Y1, Z2, 2.9f, W, H);
        const float front = clamp01((4.1f - p.z) / 2.4f), rim = 1.0f - std::fabs(Z2) / d;
        const float a = (0.25f + 0.75f * front) * (0.45f + 0.85f * rim * rim) * (0.6f + 0.4f * std::max(lvl_, 0.3f));
        dot(p.x, p.y, a, s > 0.97f ? 2 : 1);
    }
}

// Terräng: punktfält i perspektiv; varje anslag rullar en våg bort mot horisonten.
void ScreenSaver::renderTerrain(float t) {
    std::fill(buf_.begin(), buf_.end(), 0.0f);
    constexpr int GX = 150, GZ = 95;
    const float W = static_cast<float>(bw_), H = static_cast<float>(bh_);
    const double now = juce::Time::getMillisecondCounterHiRes();
    for (int iz = GZ - 1; iz >= 0; --iz) {
        const float zf = static_cast<float>(iz) / (GZ - 1), z = 0.35f + zf * 6.0f;
        float waveH = 0.0f;   // vågornas bidrag beror bara på avståndet (z)
        for (const auto& w : waves_) {
            const float age = static_cast<float>((now - w.ms) / 1000.0);
            if (age < 0.0f || age > 7.0f) continue;
            const float dz = z - (0.6f + age * 2.2f);
            waveH += w.amp * 0.55f * std::exp(-dz * dz * 1.5f) * std::exp(-age * 0.35f);
        }
        const float fog = std::pow(1.0f - zf, 0.9f);
        for (int ix = 0; ix < GX; ++ix) {
            const float xf = static_cast<float>(ix) / (GX - 1) - 0.5f, x = xf * (2.2f + z * 1.5f);
            float h = 0.32f * n3(x * 0.55f + t * 0.05f, z * 0.45f - t * 0.12f, t * 0.08f) + 0.12f * n3(x * 1.7f, z * 1.4f + t * 0.2f, 1.3f);
            h += waveH * (0.7f + 0.3f * std::sin(x * 2.2f));
            const float y = 0.55f - h * (0.7f + 0.7f * lvl_) + z * 0.02f;
            const auto p = project(x, y - z * 0.22f, z, 0.5f, W, H);
            const float lift = clamp01(0.45f + 1.2f * h);
            dot(p.x, p.y, (0.12f + 0.85f * fog) * (0.35f + 0.8f * lift), fog > 0.55f ? 2 : 1);
        }
    }
}

// Form: sfär av punktringar med lober – antalet byts vid anslag, djupet följer nivån.
void ScreenSaver::renderForm(float t) {
    std::fill(buf_.begin(), buf_.end(), 0.0f);
    const float target = 2.0f + static_cast<float>(onsets_ % 4);
    lobes_ += (target - lobes_) * 0.01f;
    depth_ += ((0.08f + 0.32f * lvl_) - depth_) * 0.06f;
    const float rotY = t * 0.22f, rotX = 0.5f + 0.25f * std::sin(t * 0.13f);
    const float cy = std::cos(rotY), sy = std::sin(rotY), cx = std::cos(rotX), sx = std::sin(rotX);
    const float W = static_cast<float>(bw_), H = static_cast<float>(bh_);
    constexpr int RINGS = 84, PER = 170;
    for (int r = 1; r < RINGS; ++r) {
        const float th = juce::MathConstants<float>::pi * static_cast<float>(r) / RINGS, st = std::sin(th), ct = std::cos(th);
        const float ringMod = std::sin((lobes_ - 1.0f) * th + t * 0.4f);
        for (int k = 0; k < PER; ++k) {
            const float ph = juce::MathConstants<float>::twoPi * static_cast<float>(k) / PER;
            const float d = 1.0f + depth_ * std::sin(lobes_ * ph + t * 0.6f) * ringMod + 0.04f * high_ * std::sin(ph * 17 + th * 13 + t * 4);
            const float x = st * std::cos(ph) * d, y = ct * d, z = st * std::sin(ph) * d;
            const float X1 = x * cy + z * sy, Z1 = -x * sy + z * cy, Y1 = y * cx - Z1 * sx, Z2 = y * sx + Z1 * cx;
            const auto p = project(X1, Y1, Z2, 2.8f, W, H);
            const float front = clamp01((4.0f - p.z) / 2.4f);
            dot(p.x, p.y, 0.16f + 0.84f * front * front, front > 0.6f ? 2 : 1);
        }
    }
}

// ---------------------------------------------------------------------------
// Alltid mörk (som referensbilderna): nästan svart bakgrund och appens ljusa bläck.
const juce::Colour ScreenSaver::kBackground { 0xff0b0b0b };
const juce::Colour ScreenSaver::kInk { 0xffedeae3 };

void ScreenSaver::toImage() {
    std::array<juce::PixelRGB, 256> lut;
    for (int i = 0; i < 256; ++i) {
        const auto c = kBackground.interpolatedWith(kInk, static_cast<float>(i) / 255.0f);
        lut[size_t(i)].setARGB(255, c.getRed(), c.getGreen(), c.getBlue());
    }
    juce::Image::BitmapData bd(image_, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < bh_; ++y) {
        const float* src = buf_.data() + y * bw_;
        auto* dst = bd.getLinePointer(y);
        for (int x = 0; x < bw_; ++x) {
            const int v = std::min(255, static_cast<int>(src[x] * 255.0f));
            *reinterpret_cast<juce::PixelRGB*>(dst + x * bd.pixelStride) = lut[size_t(v)];
        }
    }
}

void ScreenSaver::timerCallback() {
    if (!running_ || buf_.empty()) return;
    analyse();
    const float t = static_cast<float>((juce::Time::getMillisecondCounterHiRes() - startMs_) / 1000.0);
    switch (mode_) {
        case Streams: renderStreams(t); break;
        case Blob:    renderBlob(t); break;
        case Terrain: renderTerrain(t); break;
        case Form:    renderForm(t); break;
        default: break;
    }
    toImage();
    ++frame_;
    repaint();
}

void ScreenSaver::paint(juce::Graphics& g) {
    g.fillAll(kBackground);
    if (image_.isValid()) g.drawImageAt(image_, 0, 0);
}

} // namespace ui
