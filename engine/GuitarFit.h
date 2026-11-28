#pragma once
// Gitarrens plats i bandet: översätter en låtanalys (hela mixen) till inställningar som får
// en akustisk gitarr att passa in – inte låta som hela låten.
//
// Tanken (vanlig mixpraxis för akustisk gitarr i band):
//  - Botten:   bas och kick äger det under ~150 Hz. Ju mer bas i låten, desto högre
//              lågskärning och lite basavsänkning på gitarren. Glest arrangemang (lite bas)
//              -> gitarren får bära botten själv.
//  - Lågmitt:  är mixen full kring 200–400 Hz sänks gitarren där (mindre grötigt).
//  - Närvaro:  annars en liten höjning i den minst upptagna delen av 1,6–5 kHz – en "ficka"
//              bredvid sången där gitarren hörs utan att höjas i nivå.
//  - Diskant:  mörk mix -> gitarren står för luften; ljus mix (mycket cymbaler) -> låt bli.
//  - Rum:      lite torrare än låten och med predelay efter tempot, så att anslaget hörs.
//  - Dynamik:  tät, komprimerad låt -> jämnare gitarr (sustain); dynamisk låt -> naturlig.
//  - Bredd:    gitarren ligger smal i bandet – ingen widener.
//  - Tempo:    delayn får låtens tempo (avstängd, slå på vid behov).
// Ren funktion utan beroenden – testas i tests/.

#include "SongAnalyzer.h"
#include <string>

namespace gfx {

struct GuitarFit {
    // EQ (motsvarar gfx::Eq: lågskärning, basshelf 120 Hz, mittband, diskantshelf 6 kHz)
    float lowCutHz = 100.0f, lowShelfDb = 0.0f, midHz = 800.0f, midDb = 0.0f, highDb = 0.0f;
    // Reverb
    float reverbDecaySec = 1.2f, reverbMix = 0.10f, reverbPredelayMs = 25.0f;
    // Sustain (kompressor)
    bool sustain = false;
    float sustainAmount = 0.0f, sustainMix = 0.6f;
    // Delay (tempo från låten; läggs i presetet men avstängd)
    float delayBpm = 0.0f;
    int delayDivision = 1;   // 1/8 punkterad
    // Förklaringar (UTF-8, svenska)
    std::string spaceText, roomText, tempoText, dynamicsText;
};

GuitarFit fitGuitarToSong(const SongAnalysis& song);

// Samma sak från separerade stämmor (StemSplitter/Demucs): bättre underlag.
//  band  = analys av bas + trummor + sång tillsammans – bandets plats UTAN låtens egen
//          gitarr/klaviatur, så att de inte räknas som upptagen plats.
//  comp  = analys av "övrigt"-stämman (gitarr, klaviatur) – rummet mäts där, alltså hur
//          kompet ligger i inspelningen, i stället för på hela mixen (där sångens reverb dominerar).
//  bpm   = tempo från StemSplitters slaganalys (0 = använd bandets uppskattning).
GuitarFit fitGuitarToStems(const SongAnalysis& band, const SongAnalysis& comp, float bpm);

} // namespace gfx
