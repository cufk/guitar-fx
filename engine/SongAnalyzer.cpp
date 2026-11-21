#include "SongAnalyzer.h"
#include "dsp/core/FFT.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <numeric>

namespace gfx {

namespace {

constexpr double kPi = 3.14159265358979323846;

float percentile(std::vector<float> v, float p) {
    if (v.empty()) return 0.0f;
    const size_t k = static_cast<size_t>(std::clamp(p, 0.0f, 1.0f) * static_cast<float>(v.size() - 1));
    std::nth_element(v.begin(), v.begin() + static_cast<long>(k), v.end());
    return v[k];
}

float lerpClamp(float x, float x0, float x1, float y0, float y1) {
    const float t = std::clamp((x - x0) / (x1 - x0), 0.0f, 1.0f);
    return y0 + t * (y1 - y0);
}

std::string fmt(const char* f, double a, double b = 0.0, double c = 0.0) {
    char buf[200];
    std::snprintf(buf, sizeof buf, f, a, b, c);
    std::string s(buf);
    // Decimalkomma – bara mellan siffror, så att meningar fortfarande slutar med punkt.
    for (size_t i = 1; i + 1 < s.size(); ++i)
        if (s[i] == '.' && std::isdigit(static_cast<unsigned char>(s[i - 1])) && std::isdigit(static_cast<unsigned char>(s[i + 1]))) s[i] = ',';
    return s;
}

std::string signedDb(float db) {
    if (std::fabs(db) < 0.5f) return "0 dB";
    return fmt(db > 0 ? "+%.0f dB" : "\xe2\x88\x92%.0f dB", std::fabs(db));   // äkta minustecken
}

} // namespace

SongAnalysis analyseSong(const float* left, const float* right, int numSamples, double sr,
                         const std::function<bool(float)>& progress) {
    SongAnalysis a;
    if (!left || numSamples < static_cast<int>(sr * 2.0) || sr < 8000.0) return a;
    a.seconds = static_cast<float>(numSamples / sr);
    auto report = [&](float p) { return !progress || progress(p); };

    // ------------------------------------------------------------------ 1. Spektrum
    constexpr int kN = 4096, kHop = 4096;
    FFT fft;
    fft.prepare(kN);
    std::vector<std::complex<float>> buf(kN);
    std::vector<float> window(kN);
    for (int i = 0; i < kN; ++i) window[size_t(i)] = static_cast<float>(0.5 - 0.5 * std::cos(2.0 * kPi * i / (kN - 1)));
    std::vector<double> power(kN / 2, 0.0);
    int frames = 0;
    for (int start = 0; start + kN <= numSamples; start += kHop) {
        for (int i = 0; i < kN; ++i) {
            const float m = right ? 0.5f * (left[start + i] + right[start + i]) : left[start + i];
            buf[size_t(i)] = { m * window[size_t(i)], 0.0f };
        }
        fft.forward(buf.data());
        for (int k = 1; k < kN / 2; ++k) power[size_t(k)] += std::norm(buf[size_t(k)]);
        ++frames;
        if ((frames & 31) == 0 && !report(0.5f * static_cast<float>(start) / static_cast<float>(numSamples))) return {};
    }
    if (frames == 0) return a;

    // Tredjedelsoktavband 50 Hz – 12 kHz (dB), linjär anpassning mot log2(f) i 100 Hz – 10 kHz.
    struct Band { double hz, db; };
    std::vector<Band> bands;
    for (double fc = 50.0; fc <= 12000.0; fc *= std::pow(2.0, 1.0 / 3.0)) {
        const double lo = fc / std::pow(2.0, 1.0 / 6.0), hi = fc * std::pow(2.0, 1.0 / 6.0);
        const int k0 = std::max(1, static_cast<int>(lo * kN / sr)), k1 = std::min(kN / 2 - 1, static_cast<int>(hi * kN / sr));
        if (k1 < k0) continue;
        double e = 0.0;
        for (int k = k0; k <= k1; ++k) e += power[size_t(k)];
        e /= (k1 - k0 + 1);
        bands.push_back({ fc, 10.0 * std::log10(e / frames + 1e-20) });
    }
    double sx = 0, sy = 0, sxx = 0, sxy = 0; int cnt = 0;
    for (const auto& b : bands) if (b.hz >= 100.0 && b.hz <= 10000.0) {
        const double x = std::log2(b.hz);
        sx += x; sy += b.db; sxx += x * x; sxy += x * b.db; ++cnt;
    }
    const double slope = (cnt * sxy - sx * sy) / std::max(1e-9, cnt * sxx - sx * sx);
    const double icept = (sy - slope * sx) / std::max(1, cnt);
    // Bas och diskant jämförs med en fast referens – en typisk mix som lutar ca −4,5 dB per
    // oktav – förankrad i mellanregistret (500 Hz – 2 kHz). En anpassad linje skulle följa
    // med låtens egen lutning och dölja just det vi vill mäta.
    double anchor = 0.0; int na = 0;
    for (const auto& b : bands) if (b.hz >= 500.0 && b.hz <= 2000.0) { anchor += b.db + 4.5 * std::log2(b.hz / 1000.0); ++na; }
    anchor = na ? anchor / na : 0.0;
    auto devRef = [&](double f0, double f1) {
        double sum = 0; int n = 0;
        for (const auto& b : bands) if (b.hz >= f0 && b.hz <= f1) { sum += b.db - (anchor - 4.5 * std::log2(b.hz / 1000.0)); ++n; }
        return n ? static_cast<float>(sum / n) : 0.0f;
    };
    a.lowGainDb  = std::clamp(0.5f * devRef(70.0, 200.0), -6.0f, 6.0f);
    a.highGainDb = std::clamp(0.5f * devRef(4500.0, 11000.0), -6.0f, 6.0f);
    float bestDev = 0.0f;
    for (const auto& b : bands) if (b.hz >= 250.0 && b.hz <= 3000.0) {
        const float d = static_cast<float>(b.db - (icept + slope * std::log2(b.hz)));
        if (std::fabs(d) > std::fabs(bestDev)) { bestDev = d; a.midHz = static_cast<float>(b.hz); }
    }
    a.midGainDb = std::clamp(0.5f * bestDev, -6.0f, 6.0f);
    // Lite energi under 80 Hz relativt linjen => skär lågt (typiskt för akustisk gitarr i mix).
    const float subDev = devRef(45.0, 80.0) - devRef(100.0, 200.0);
    a.lowCutHz = subDev < -6.0f ? 120.0f : subDev < -2.0f ? 90.0f : 60.0f;
    if (!report(0.55f)) return {};

    // ------------------------------------------------------------------ 2. Korttidsnivå (10 ms)
    const int hop = std::max(1, static_cast<int>(sr * 0.01));
    const int nFrames = numSamples / hop;
    std::vector<float> envDb(static_cast<size_t>(nFrames));
    double ms = 0.0, ss = 0.0, msSum = 0.0, lrSum = 0.0, llSum = 0.0, rrSum = 0.0;
    for (int f = 0; f < nFrames; ++f) {
        double e = 0.0;
        for (int i = f * hop; i < (f + 1) * hop; ++i) {
            const double l = left[i], r = right ? right[i] : left[i];
            const double m = 0.5 * (l + r), s = 0.5 * (l - r);
            e += m * m;
            ms += m * m; ss += s * s;
            lrSum += l * r; llSum += l * l; rrSum += r * r;
        }
        msSum += e;
        envDb[size_t(f)] = static_cast<float>(10.0 * std::log10(e / hop + 1e-12));
    }
    if (!report(0.7f)) return {};

    // ------------------------------------------------------------------ 3. Bredd
    a.sideToMid = static_cast<float>(ms > 0 ? ss / ms : 0.0);
    a.correlation = static_cast<float>(llSum > 0 && rrSum > 0 ? lrSum / std::sqrt(llSum * rrSum) : 1.0);
    a.width = std::clamp(lerpClamp(a.sideToMid, 0.02f, 0.35f, 0.0f, 1.0f), 0.0f, 1.0f);

    // ------------------------------------------------------------------ 4. Dynamik
    // Bara aktiva partier (inom 40 dB från toppen) – pauser och utklingning räknas inte.
    const float top = percentile(envDb, 0.99f);
    std::vector<float> active;
    for (float d : envDb) if (d > top - 40.0f) active.push_back(d);
    // 300 ms glidande medel (korttidsnivå) innan spridningen mäts.
    std::vector<float> st;
    for (size_t i = 0; i + 30 <= active.size(); i += 10) {
        double s = 0; for (size_t k = i; k < i + 30; ++k) s += std::pow(10.0, active[k] / 10.0);
        st.push_back(static_cast<float>(10.0 * std::log10(s / 30.0 + 1e-12)));
    }
    a.loudnessRangeDb = percentile(st, 0.95f) - percentile(st, 0.10f);
    a.compression = lerpClamp(a.loudnessRangeDb, 14.0f, 4.0f, 0.0f, 1.0f);

    // ------------------------------------------------------------------ 5. Anslag, tempo och rum
    std::vector<float> flux(static_cast<size_t>(nFrames), 0.0f);
    for (int f = 1; f < nFrames; ++f) flux[size_t(f)] = std::max(0.0f, envDb[size_t(f)] - envDb[size_t(f - 1)]);
    // Tempo: autokorrelation av anslagsstyrkan (100 bilder/s), 60–180 BPM.
    {
        const float meanF = std::accumulate(flux.begin(), flux.end(), 0.0f) / std::max(1, nFrames);
        std::vector<float> fz(flux.size());
        for (size_t i = 0; i < flux.size(); ++i) fz[i] = flux[i] - meanF;
        double best = -1e30; int bestLag = 0;
        const int lag0 = static_cast<int>(100.0 * 60.0 / 180.0), lag1 = static_cast<int>(100.0 * 60.0 / 60.0);
        std::vector<double> ac(static_cast<size_t>(lag1 + 2), 0.0);
        for (int lag = lag0 - 1; lag <= lag1 + 1; ++lag) {
            double s = 0.0;
            for (size_t i = 0; i + static_cast<size_t>(lag) < fz.size(); ++i) s += double(fz[i]) * fz[i + static_cast<size_t>(lag)];
            ac[size_t(lag)] = s;
        }
        for (int lag = lag0; lag <= lag1; ++lag) {
            // Lätt viktning mot 80–140 BPM (vanligast) så att halva/dubbla tempot sällan vinner.
            const double bpm = 6000.0 / lag;
            const double w = 1.0 - 0.25 * std::min(1.0, std::fabs(std::log2(bpm / 110.0)));
            if (ac[size_t(lag)] * w > best) { best = ac[size_t(lag)] * w; bestLag = lag; }
        }
        if (bestLag > 0 && best > 0.0) {
            double frac = 0.0;
            const double y0 = ac[size_t(bestLag - 1)], y1 = ac[size_t(bestLag)], y2 = ac[size_t(bestLag + 1)];
            const double den = y0 - 2.0 * y1 + y2;
            if (std::fabs(den) > 1e-12) frac = std::clamp(0.5 * (y0 - y2) / den, -0.5, 0.5);
            a.bpm = static_cast<float>(6000.0 / (bestLag + frac) * (sr / (100.0 * hop)));   // korrigera för verklig bildtakt
        }
    }
    // Avklingning (rum). Rummets efterklang syns bäst där tonerna får klinga ut:
    //  1. I pauser på minst 0,8 s (frasslut, låtens slut) mäts den sena delen av utklingningen
    //     (0,4–2,5 s efter anslaget) – där dominerar rummet över tonens egen avklingning.
    //  2. Finns inga pauser mäts 150 ms fram till nästa anslag (tätt spelade låtar).
    {
        const float onsetThr = std::max(3.0f, percentile(flux, 0.97f));
        const float noiseFloor = percentile(envDb, 0.05f) + 6.0f;
        std::vector<int> onsets;
        for (int f = 1; f < nFrames; ++f)
            if (flux[size_t(f)] >= onsetThr && (onsets.empty() || f - onsets.back() > 8)) onsets.push_back(f);

        auto fitSlope = [&](int peak, int w0, int w1, int minN) -> float {
            double sx2 = 0, sy2 = 0, sxx2 = 0, sxy2 = 0; int n = 0;
            for (int k = w0; k < w1 && k < nFrames; ++k) {
                const float lv = envDb[size_t(k)];
                if (lv < top - 65.0f || lv < noiseFloor) break;   // under brusgolvet
                const double x = (k - peak) * 0.01;
                sx2 += x; sy2 += lv; sxx2 += x * x; sxy2 += x * lv; ++n;
            }
            if (n < minN) return 0.0f;
            const double sl = (n * sxy2 - sx2 * sy2) / std::max(1e-9, n * sxx2 - sx2 * sx2);
            return sl < 0.0 ? static_cast<float>(sl) : 0.0f;
        };
        auto peakAfter = [&](int f) {
            int peak = f;
            for (int k = f; k < std::min(nFrames, f + 5); ++k) if (envDb[size_t(k)] > envDb[size_t(peak)]) peak = k;
            return peak;
        };

        std::vector<float> gapSlopes, slopes;
        for (size_t i = 0; i < onsets.size(); ++i) {
            const int peak = peakAfter(onsets[i]);
            if (envDb[size_t(peak)] < top - 30.0f) continue;
            const int next = i + 1 < onsets.size() ? onsets[i + 1] : nFrames;
            if (next - peak >= 80) {
                if (const float sl = fitSlope(peak, peak + 40, std::min(next - 3, peak + 250), 20); sl < 0.0f) gapSlopes.push_back(sl);
            }
            if (const float sl = fitSlope(peak, peak + 15, std::min(next - 3, peak + 100), 12); sl < 0.0f) slopes.push_back(sl);
        }
        if (!gapSlopes.empty()) slopes = gapSlopes;
        a.decayDbPerSec = slopes.empty() ? -30.0f : percentile(slopes, 0.5f);
        a.reverbDecaySec = std::clamp(60.0f / std::max(1.0f, -a.decayDbPerSec), 0.6f, 5.0f);
        // Hur mycket som ligger kvar mellan anslagen: golv (p20) relativt toppar (p95).
        a.floorToPeakDb = percentile(active, 0.20f) - percentile(active, 0.95f);
        a.reverbMix = lerpClamp(a.floorToPeakDb, -30.0f, -8.0f, 0.10f, 0.45f);
        // Större rum (lång efterklang) -> lite längre predelay så att anslaget hörs.
        a.reverbPredelayMs = lerpClamp(a.reverbDecaySec, 0.8f, 4.0f, 8.0f, 35.0f);
    }
    if (!report(1.0f)) return {};

    // ------------------------------------------------------------------ Förklaringar
    auto describeTone = [&] {
        std::string s;
        if (a.lowGainDb > 1.0f) s += "Fyllig bas"; else if (a.lowGainDb < -1.0f) s += "Lätt i basen"; else s += "Neutral bas";
        if (std::fabs(a.midGainDb) > 1.0f) s += fmt(a.midGainDb > 0 ? ", framträdande mitt kring %.0f Hz" : ", urgröpt kring %.0f Hz", a.midHz);
        if (a.highGainDb > 1.0f) s += ", ljus diskant"; else if (a.highGainDb < -1.0f) s += ", mjuk diskant";
        s += ". EQ: bas " + signedDb(a.lowGainDb) + ", mitt " + signedDb(a.midGainDb) + ", diskant " + signedDb(a.highGainDb) + ".";
        return s;
    };
    a.toneText = describeTone();
    a.roomText = (a.reverbDecaySec > 2.8f ? "Stort rum" : a.reverbDecaySec > 1.5f ? "Mellanstort rum" : "Litet, torrt rum")
               + fmt(" – efterklang ca %.1f s, mix %.0f %%.", a.reverbDecaySec, a.reverbMix * 100.0);
    a.widthText = a.width > 0.6f ? "Brett stereoljud – widener föreslås."
                : a.width > 0.25f ? "Måttligt brett – lätt widener föreslås."
                : "Smalt, nästan mono – ingen widener.";
    a.dynamicsText = a.compression > 0.6f ? fmt("Jämn nivå (%.0f dB spridning) – sustain föreslås.", a.loudnessRangeDb)
                   : a.compression > 0.3f ? fmt("Något komprimerat (%.0f dB spridning) – lätt sustain.", a.loudnessRangeDb)
                   : fmt("Dynamiskt (%.0f dB spridning) – ingen sustain.", a.loudnessRangeDb);
    a.valid = true;
    return a;
}

} // namespace gfx
