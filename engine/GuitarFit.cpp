#include "GuitarFit.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace gfx {

namespace {
std::string fmt(const char* f, double a) { char b[160]; std::snprintf(b, sizeof b, f, a); return b; }
std::string fmt2(const char* f, double a, double c) { char b[160]; std::snprintf(b, sizeof b, f, a, c); return b; }
std::string db(float v) {   // "+2 dB" / "−3 dB" (riktigt minustecken)
    char b[32]; std::snprintf(b, sizeof b, "%.0f dB", std::fabs(v));
    return std::string(v < 0 ? "\xE2\x88\x92" : "+") + b;
}
std::string hz(float f) {
    if (f >= 1000.0f) { char b[32]; std::snprintf(b, sizeof b, "%.1f kHz", f / 1000.0); std::string s = b; for (auto& c : s) if (c == '.') c = ','; return s; }
    return fmt("%.0f Hz", f);
}
} // namespace

GuitarFit fitGuitarToSong(const SongAnalysis& s) {
    GuitarFit f;
    if (!s.valid) return f;

    // ---- Botten: lämna plats åt bas och kick. En tung kick/subbas räknas också (rock, pop).
    const float bottomDb = std::max(s.bassDb, 0.7f * s.subDb);
    const bool sparse = bottomDb < -6.0f;   // glest arrangemang utan bas (t.ex. gitarr + sång)
    if (bottomDb > 2.0f)       { f.lowCutHz = 150.0f; f.lowShelfDb = -3.0f; }
    else if (bottomDb > -1.0f) { f.lowCutHz = 120.0f; f.lowShelfDb = -1.5f; }
    else if (!sparse)          { f.lowCutHz = 100.0f; f.lowShelfDb = 0.0f; }
    else                       { f.lowCutHz = 85.0f;  f.lowShelfDb = 0.0f; }

    std::string bottom;
    if (bottomDb > 2.0f)       bottom = s.subDb > s.bassDb + 3.0f ? "Kick och subbas fyller botten" : "Bas och kick fyller botten";
    else if (bottomDb > -1.0f) bottom = "Normalt med bas i låten";
    else if (!sparse)          bottom = "Lite bas i låten";
    else                       bottom = "Nästan ingen bas i låten – gitarren får bära botten";
    bottom += " – lågskärning " + hz(f.lowCutHz) + (f.lowShelfDb < 0 ? ", bas " + db(f.lowShelfDb) : std::string()) + ".";

    // ---- Mitten: ta bort gröt där mixen är full, annars hitta en ficka bredvid sången
    std::string middle;
    // (Gröt-sänkningen bara med bas i låten – i ett glest arrangemang är lågmitten gitarren själv.)
    if (s.lowMidDb > 1.5f && !sparse) {
        f.midHz = 300.0f;
        f.midDb = -std::clamp(s.lowMidDb, 2.0f, 4.0f);
        middle = " Tätt kring 200–400 Hz: " + db(f.midDb) + " vid " + hz(f.midHz) + " så att gitarren inte blir grötig.";
    } else if (s.pocketDb < -1.5f) {
        f.midHz = s.pocketHz;
        f.midDb = std::clamp(-0.6f * s.pocketDb, 1.5f, 3.0f);
        middle = " Luft i mixen kring " + hz(f.midHz) + ": " + db(f.midDb) + " där, så att gitarren hörs bredvid sången.";
    } else {
        f.midHz = 800.0f;
        f.midDb = 0.0f;
        middle = " Mitten är jämnt fylld – ingen ändring där.";
    }

    // ---- Diskant: luft om mixen är mörk, försiktigt om den är ljus
    if (s.airDb < -2.0f)     f.highDb = 3.0f;
    else if (s.airDb < 0.0f) f.highDb = 2.0f;
    else if (s.airDb < 2.0f) f.highDb = 1.0f;
    else                     f.highDb = 0.0f;
    std::string top = f.highDb >= 2.0f ? " Mörk mix: diskant " + db(f.highDb) + " – gitarren står för luften."
                    : f.highDb > 0.0f ? " Diskant " + db(f.highDb) + "."
                    : " Ljus mix – diskanten lämnas.";
    f.spaceText = bottom + middle + top;

    // ---- Rum: lite torrare än låten, predelay efter tempot
    if (s.roomMeasure == SongAnalysis::RoomMeasure::Default) {
        f.reverbDecaySec = 1.2f;
        f.reverbMix = 0.10f;
    } else {
        f.reverbDecaySec = std::clamp(0.8f * s.reverbDecaySec, 0.7f, 2.5f);
        f.reverbMix = std::clamp(0.6f * s.reverbMix, 0.06f, sparse ? 0.20f : 0.15f);   // fullt band: högst 15 %
    }
    f.reverbPredelayMs = s.bpm > 0.0f ? std::clamp(60000.0f / s.bpm / 16.0f, 15.0f, 40.0f) : 25.0f;
    f.roomText = (s.roomMeasure == SongAnalysis::RoomMeasure::Default
                     ? std::string("Rummet gick inte att mäta säkert – ett diskret rum")
                     : fmt("Låtens rum: ca %.1f s", s.reverbDecaySec) + ". Gitarren lite torrare")
               + fmt2(": %.1f s, mix %.0f %%", f.reverbDecaySec, f.reverbMix * 100.0)
               + fmt(", predelay %.0f ms så att anslaget hörs i bandet.", f.reverbPredelayMs);

    // ---- Dynamik: jämn gitarr i en tät låt
    if (s.compression > 0.6f)      { f.sustain = true; f.sustainAmount = 0.5f; f.sustainMix = 0.6f; }
    else if (s.compression > 0.3f) { f.sustain = true; f.sustainAmount = 0.3f; f.sustainMix = 0.5f; }
    f.dynamicsText = f.sustainAmount >= 0.5f
        ? fmt("Tät, jämn låt (%.0f dB spridning) – sustain gör gitarren jämn så att den inte försvinner.", s.loudnessRangeDb)
        : f.sustain ? fmt("Något komprimerad låt (%.0f dB spridning) – lätt sustain.", s.loudnessRangeDb)
                    : fmt("Dynamisk låt (%.0f dB spridning) – gitarren får vara naturlig, ingen sustain.", s.loudnessRangeDb);

    // ---- Tempo
    if (s.bpm > 0.0f) {
        f.delayBpm = std::round(std::clamp(s.bpm, 40.0f, 240.0f));
        f.delayDivision = 1;
        f.tempoText = fmt("Delayn får låtens tempo (%.0f BPM, 1/8 punkterad) men är avstängd – slå på den om låten har eko. "
                          "Gitarren ligger smal i bandet, så ingen widener.", f.delayBpm);
    } else {
        f.tempoText = "Tempot gick inte att bestämma. Gitarren ligger smal i bandet, så ingen widener.";
    }
    // Decimalkomma i förklaringarna.
    for (auto* t : { &f.roomText, &f.dynamicsText }) {
        for (size_t i = 1; i + 1 < t->size(); ++i)
            if ((*t)[i] == '.' && std::isdigit(static_cast<unsigned char>((*t)[i - 1])) && std::isdigit(static_cast<unsigned char>((*t)[i + 1]))) (*t)[i] = ',';
    }
    return f;
}

GuitarFit fitGuitarToStems(const SongAnalysis& band, const SongAnalysis& comp, float bpm) {
    SongAnalysis b = band;
    if (bpm > 0.0f) b.bpm = bpm;
    if (comp.valid && comp.roomMeasure != SongAnalysis::RoomMeasure::Default) {
        // Rummet från kompet: så låter gitarren/kompet i inspelningen.
        b.roomMeasure = comp.roomMeasure;
        b.reverbDecaySec = comp.reverbDecaySec;
        b.reverbMix = comp.reverbMix;
    }
    GuitarFit f = fitGuitarToSong(b);
    if (!b.valid) return f;
    f.spaceText = "Från stämmorna (bas, trummor, sång – utan låtens egen gitarr): " + f.spaceText;
    if (comp.valid && comp.roomMeasure != SongAnalysis::RoomMeasure::Default) {
        const auto pos = f.roomText.find("Låtens rum");
        if (pos != std::string::npos) f.roomText.replace(pos, std::string("Låtens rum").size(), "Kompets rum i inspelningen");
    }
    if (bpm > 0.0f) f.tempoText = "Tempo från StemSplitter. " + f.tempoText;
    return f;
}

} // namespace gfx
