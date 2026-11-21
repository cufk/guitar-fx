#include "Cabinet.h"
#include "../core/Biquad.h"
#include "../core/DspUtils.h"
#include <algorithm>
#include <cmath>

namespace gfx {

namespace {

// Bandbegränsad omsampling med Kaiser-fönstrad sinc. Ej realtid.
std::vector<float> resample(const std::vector<float>& in, double fromRate, double toRate) {
    if (std::fabs(fromRate - toRate) < 0.5 || in.empty()) return in;
    const double ratio = toRate / fromRate;
    const double cutoff = std::min(1.0, ratio);      // lågpass vid nedsampling
    constexpr int kHalf = 24;                         // fönsterbredd (sidor)
    const double halfWidth = kHalf / cutoff;
    const double pi = 3.14159265358979323846;
    const size_t outLen = static_cast<size_t>(std::ceil(static_cast<double>(in.size()) * ratio));
    std::vector<float> out(outLen, 0.0f);
    for (size_t i = 0; i < outLen; ++i) {
        const double t = static_cast<double>(i) / ratio;
        const long lo = static_cast<long>(std::ceil(t - halfWidth));
        const long hi = static_cast<long>(std::floor(t + halfWidth));
        double acc = 0.0;
        for (long j = std::max(0L, lo); j <= std::min(static_cast<long>(in.size()) - 1, hi); ++j) {
            const double x = (t - static_cast<double>(j)) * cutoff;
            const double sinc = std::fabs(x) < 1e-9 ? 1.0 : std::sin(pi * x) / (pi * x);
            const double r = (t - static_cast<double>(j)) / halfWidth;
            const double w = 0.5 + 0.5 * std::cos(pi * std::clamp(r, -1.0, 1.0)); // Hann
            acc += in[static_cast<size_t>(j)] * cutoff * sinc * w;
        }
        out[i] = static_cast<float>(acc);
    }
    return out;
}

} // namespace

Cabinet::Cabinet() {
    sourceIr_ = builtInImpulseResponse();
    sourceRate_ = 48000.0;
    sourceName_ = "Inbyggd (1x12)";
}

Cabinet::~Cabinet() {
    delete current_;
    delete pending_.exchange(nullptr);
    delete retired_.exchange(nullptr);
}

std::vector<float> Cabinet::builtInImpulseResponse() {
    // Syntetiskt svar: basresonans, mittdal, presence-topp och brant
    // diskantavrullning – grovt som en sluten 1x12 med dynamisk mikrofon.
    constexpr double sr = 48000.0;
    constexpr int n = 2048;
    Biquad f[6];
    f[0].setCoeffs(BiquadCoeffs::highpass(sr, 75.0, 0.9));
    f[1].setCoeffs(BiquadCoeffs::peak(sr, 115.0, 1.4, 4.0));
    f[2].setCoeffs(BiquadCoeffs::peak(sr, 450.0, 0.9, -3.5));
    f[3].setCoeffs(BiquadCoeffs::peak(sr, 2400.0, 1.6, 5.0));
    f[4].setCoeffs(BiquadCoeffs::lowpass(sr, 5200.0, 0.8));
    f[5].setCoeffs(BiquadCoeffs::lowpass(sr, 6500.0, 0.6));
    std::vector<float> ir(n, 0.0f);
    for (int i = 0; i < n; ++i) {
        float x = i == 0 ? 1.0f : 0.0f;
        for (auto& b : f) x = b.processSample(0, x);
        ir[static_cast<size_t>(i)] = x;
    }
    return ir;
}

ConvolverIR* Cabinet::buildForCurrentRate() {
    auto ir = resample(sourceIr_, sourceRate_, sampleRate_.load());
    const size_t maxLen = static_cast<size_t>(maxParts_) * ConvolverIR::kPartition;
    if (ir.size() > maxLen) {
        ir.resize(maxLen);
        // Kort uttoning så att trunkeringen inte ger ett klick i svansen.
        const size_t fade = std::min<size_t>(256, maxLen);
        for (size_t i = 0; i < fade; ++i) ir[maxLen - 1 - i] *= static_cast<float>(i) / static_cast<float>(fade);
    }
    // Normalisera: RMS-förstärkning för vitt brus = 1.
    double energy = 0.0;
    for (float v : ir) energy += static_cast<double>(v) * v;
    const float scale = energy > 1e-12 ? static_cast<float>(1.0 / std::sqrt(energy)) : 0.0f;
    for (float& v : ir) v *= scale;
    return ConvolverIR::build(ir.data(), static_cast<int>(ir.size()), fft_);
}

void Cabinet::prepare(const ProcessSpec& spec) {
    std::lock_guard<std::mutex> lock(mutex_);
    sampleRate_.store(spec.sampleRate);
    numCh_ = std::min(spec.numChannels, kMaxChannels);
    fft_.prepare(ConvolverIR::kFftSize);
    maxParts_ = static_cast<int>(std::ceil(kMaxIrSeconds * spec.sampleRate / ConvolverIR::kPartition));
    conv_.assign(static_cast<size_t>(numCh_), {});
    for (auto& c : conv_) c.prepare(maxParts_);

    // Ljudet står still under prepare(): byt direkt.
    delete current_;
    delete pending_.exchange(nullptr);
    delete retired_.exchange(nullptr);
    current_ = buildForCurrentRate();

    level_.prepare(spec.sampleRate, 0.03);
    level_.setCurrentAndTarget(dbToGain(levelDb.get()));
}

void Cabinet::reset() {
    for (auto& c : conv_) c.reset();
    level_.setCurrentAndTarget(dbToGain(levelDb.get()));
}

void Cabinet::process(AudioBufferView buf) {
    // Plocka upp ny IR bara om det finns plats att lämna tillbaka den gamla.
    if (retired_.load(std::memory_order_acquire) == nullptr) {
        if (auto* p = pending_.exchange(nullptr, std::memory_order_acq_rel)) {
            retired_.store(current_, std::memory_order_release);
            current_ = p;
        }
    }

    const int nCh = std::min(buf.numChannels, static_cast<int>(conv_.size()));
    for (int ch = 0; ch < nCh; ++ch)
        conv_[static_cast<size_t>(ch)].process(current_, fft_, buf.channels[ch], buf.numSamples);

    level_.setTarget(dbToGain(levelDb.get()));
    for (int i = 0; i < buf.numSamples; ++i) {
        const float g = level_.next();
        for (int ch = 0; ch < nCh; ++ch) buf.channels[ch][i] *= g;
    }
}

void Cabinet::publish(ConvolverIR* ir) {
    delete retired_.exchange(nullptr, std::memory_order_acq_rel);
    // Om en tidigare IR aldrig hann plockas upp ägs den fortfarande av oss.
    delete pending_.exchange(ir, std::memory_order_acq_rel);
}

bool Cabinet::loadImpulseResponse(const float* data, int numSamples, double sampleRate, std::string displayName) {
    if (!data || numSamples <= 0 || !(sampleRate > 1000.0)) return false;
    std::vector<float> ir(data, data + numSamples);
    for (float v : ir) if (!isFiniteSample(v)) return false;
    // Hoppa över inledande tystnad (vanligt i IR-filer) så att svaret inte fördröjs.
    float peak = 0.0f;
    for (float v : ir) peak = std::max(peak, std::fabs(v));
    if (peak <= 0.0f) return false;
    size_t first = 0;
    while (first < ir.size() && std::fabs(ir[first]) < peak * 1.0e-3f) ++first;
    ir.erase(ir.begin(), ir.begin() + static_cast<long>(first > 0 ? first - 1 : 0));

    std::lock_guard<std::mutex> lock(mutex_);
    sourceIr_ = std::move(ir);
    sourceRate_ = sampleRate;
    sourceName_ = std::move(displayName);
    if (!conv_.empty()) publish(buildForCurrentRate());
    return true;
}

void Cabinet::useBuiltInImpulseResponse() {
    std::lock_guard<std::mutex> lock(mutex_);
    sourceIr_ = builtInImpulseResponse();
    sourceRate_ = 48000.0;
    sourceName_ = "Inbyggd (1x12)";
    if (!conv_.empty()) publish(buildForCurrentRate());
}

std::string Cabinet::impulseResponseName() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sourceName_;
}

void Cabinet::collectGarbage() {
    delete retired_.exchange(nullptr, std::memory_order_acq_rel);
}

} // namespace gfx
