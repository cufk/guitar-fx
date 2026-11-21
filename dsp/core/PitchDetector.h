#pragma once
// Tonhöjdsigenkänning för stämapparaten (YIN, de Cheveigné & Kawahara 2002).
//
//  - Klarar gitarrens hela register (ca 60–1300 Hz, även drop-stämningar).
//  - Insignalen decimeras till ~24 kHz (räcker gott för grundtonen och halverar jobbet).
//  - Parabolisk interpolation ger bråkdels-noggrannhet: < 1 cent på rena toner.
//  - Körs på GUI-tråden (inte ljudtråden); allokerar bara i prepare().

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace gfx {

struct PitchResult {
    float hz = 0.0f;          // 0 = ingen ton
    float confidence = 0.0f;  // 0..1 (1 - YIN-värdet vid toppen)
    float level = 0.0f;       // RMS i fönstret
};

class PitchDetector {
public:
    static constexpr float kMinHz = 60.0f, kMaxHz = 1300.0f;

    // windowSamples: antal samples (vid sampleRate) som analyseras per anrop.
    void prepare(double sampleRate, int windowSamples) {
        sr_ = sampleRate;
        decim_ = sampleRate > 60000.0 ? 4 : (sampleRate > 30000.0 ? 2 : 1);
        n_ = windowSamples / decim_;
        x_.assign(static_cast<size_t>(n_), 0.0f);
        const double dsr = sampleRate / decim_;
        tauMin_ = std::max(2, static_cast<int>(dsr / kMaxHz));
        tauMax_ = std::min(n_ / 2 - 1, static_cast<int>(dsr / kMinHz) + 2);
        d_.assign(static_cast<size_t>(tauMax_ + 2), 0.0f);
    }

    int windowSamples() const { return n_ * decim_; }

    PitchResult detect(const float* in, int numSamples) {
        PitchResult r;
        if (n_ <= 0 || numSamples < n_ * decim_) return r;
        // Decimera med enkel medelvärdesbildning (lågpass) och ta bort DC.
        double mean = 0.0, energy = 0.0;
        for (int i = 0; i < n_; ++i) {
            float s = 0.0f;
            for (int k = 0; k < decim_; ++k) s += in[i * decim_ + k];
            x_[size_t(i)] = s / static_cast<float>(decim_);
            mean += x_[size_t(i)];
        }
        mean /= n_;
        for (auto& v : x_) { v -= static_cast<float>(mean); energy += double(v) * v; }
        r.level = static_cast<float>(std::sqrt(energy / n_));
        if (r.level < 1.0e-4f) return r;   // tyst (ca -80 dBFS)

        // Skillnadsfunktion och kumulativt normaliserad skillnad.
        const int W = n_ - tauMax_;
        d_[0] = 1.0f;
        double running = 0.0;
        for (int tau = 1; tau <= tauMax_; ++tau) {
            double sum = 0.0;
            const float* a = x_.data();
            const float* b = x_.data() + tau;
            for (int j = 0; j < W; ++j) { const float diff = a[j] - b[j]; sum += double(diff) * diff; }
            running += sum;
            d_[size_t(tau)] = running > 0.0 ? static_cast<float>(sum * tau / running) : 1.0f;
        }

        // Första minimum under tröskeln (annars globalt minimum).
        constexpr float kThreshold = 0.12f;
        int best = -1;
        for (int tau = tauMin_; tau < tauMax_; ++tau) {
            if (d_[size_t(tau)] < kThreshold) {
                while (tau + 1 < tauMax_ && d_[size_t(tau + 1)] < d_[size_t(tau)]) ++tau;
                best = tau;
                break;
            }
        }
        if (best < 0) {
            best = tauMin_;
            for (int tau = tauMin_; tau < tauMax_; ++tau) if (d_[size_t(tau)] < d_[size_t(best)]) best = tau;
        }

        // Parabolisk interpolation runt minimum.
        double t = best;
        if (best > tauMin_ && best + 1 < tauMax_) {
            const double y0 = d_[size_t(best - 1)], y1 = d_[size_t(best)], y2 = d_[size_t(best + 1)];
            const double den = y0 - 2.0 * y1 + y2;
            if (std::fabs(den) > 1e-12) t += std::clamp(0.5 * (y0 - y2) / den, -0.5, 0.5);
        }
        r.confidence = std::clamp(1.0f - d_[size_t(best)], 0.0f, 1.0f);
        r.hz = static_cast<float>(sr_ / decim_ / t);

        // Finjustering i full samplingsfrekvens runt den grova perioden (viktigt för höga
        // toner, där en period bara är några tiotal samples efter decimeringen).
        if (decim_ > 1) {
            const int centre = static_cast<int>(std::lround(t * decim_));
            const int full = n_ * decim_;
            const int span = full - centre - 4;
            if (centre > 4 && span > full / 4) {
                double e[7];
                for (int k = -3; k <= 3; ++k) {
                    const int tau = centre + k;
                    double sum = 0.0;
                    for (int j = 0; j < span; ++j) { const double diff = double(in[j]) - in[j + tau]; sum += diff * diff; }
                    e[k + 3] = sum;
                }
                int m = 3;
                for (int k = 0; k < 7; ++k) if (e[k] < e[m]) m = k;
                if (m > 0 && m < 6) {
                    const double den = e[m - 1] - 2.0 * e[m] + e[m + 1];
                    const double frac = std::fabs(den) > 1e-18 ? std::clamp(0.5 * (e[m - 1] - e[m + 1]) / den, -0.5, 0.5) : 0.0;
                    r.hz = static_cast<float>(sr_ / (centre + (m - 3) + frac));
                }
            }
        }
        if (r.hz < kMinHz * 0.9f || r.hz > kMaxHz * 1.1f) r.hz = 0.0f;
        return r;
    }

private:
    double sr_ = 48000.0;
    int decim_ = 2, n_ = 0, tauMin_ = 2, tauMax_ = 2;
    std::vector<float> x_, d_;
};

// Ton och avvikelse i cent relativt närmaste halvton (A4 = referens).
struct NoteInfo {
    int midi = 0;             // 69 = A4
    float cents = 0.0f;       // -50..+50
    std::string name;         // t.ex. "E"
    int octave = 0;
};

inline NoteInfo noteFromHz(float hz, float referenceA4 = 440.0f) {
    static const char* kNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    NoteInfo n;
    if (hz <= 0.0f) return n;
    const double semis = 12.0 * std::log2(hz / referenceA4) + 69.0;
    n.midi = static_cast<int>(std::lround(semis));
    n.cents = static_cast<float>((semis - n.midi) * 100.0);
    n.name = kNames[((n.midi % 12) + 12) % 12];
    n.octave = n.midi / 12 - 1;
    return n;
}

inline float hzFromMidi(int midi, float referenceA4 = 440.0f) {
    return referenceA4 * std::pow(2.0f, static_cast<float>(midi - 69) / 12.0f);
}

} // namespace gfx
