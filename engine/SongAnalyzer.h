#pragma once
// Låtanalys: mäter en inspelad låt (hela mixen). Ren signalanalys – ingen nedladdning,
// inget nätverk, fungerar likadant på Mac, iPad och Raspberry Pi.
// Mätningen beskriver LÅTEN. Vad gitarren ska göra för att få plats i bandet räknas ut
// separat i engine/GuitarFit.h (gitarren ska komplettera mixen, inte härma den).
//
//  Klang    tersbandsspektrum jämfört med en referenskurva för inspelad musik: avvikelser i
//           bas / diskant blir EQ (halva avvikelsen, ±6 dB); mitten bara vid tydlig topp/dal.
//  Rum      stereo: koherens mellan vänster och höger (efterklang gör kanalerna osammanhängande);
//           mono: sen avklingning i fria avklingningar; för lite underlag -> försiktigt standardval.
//  Bredd    sida/mitt-förhållande och korrelation mellan vänster och höger -> widener.
//  Dynamik  spridning i korttidsnivå (p95 - p10) -> hur komprimerat -> sustain.
//  Tempo    autokorrelation av anslagsstyrkan, 60–180 BPM.
//
// Analysen är en kvalificerad gissning: hela mixen påverkar (sång, trummor), så
// förslagen är en startpunkt att justera med öronen.

#include <functional>
#include <string>
#include <vector>

namespace gfx {

struct SongAnalysis {
    bool valid = false;
    float seconds = 0.0f;
    float bpm = 0.0f;

    // Klang (låtens avvikelse från referenskurvan, halverad – beskriver låten, inte gitarren)
    float lowCutHz = 80.0f;
    float lowGainDb = 0.0f, midGainDb = 0.0f, midHz = 800.0f, highGainDb = 0.0f;

    // Låtens frekvensbalans per område, dB relativt referenskurvan för inspelad musik
    // (förankrad i 500 Hz – 2 kHz). Positivt = mer än vanligt, dvs. området är upptaget.
    float subDb = 0.0f;        // 40–70 Hz (kick, subbas)
    float bassDb = 0.0f;       // 70–160 Hz (bas, kick)
    float lowMidDb = 0.0f;     // 200–400 Hz (grötighet)
    float presenceDb = 0.0f;   // 1,6–4 kHz (sång, närvaro)
    float airDb = 0.0f;        // 5–11 kHz (cymbaler, luft)
    // Minst upptagna frekvensen i 2–5 kHz och hur mycket lägre den ligger än området i
    // snitt (negativt = en ficka där gitarren kan höras).
    float pocketHz = 3000.0f, pocketDb = 0.0f;

    // Rum
    float reverbDecaySec = 1.5f, reverbMix = 0.2f, reverbPredelayMs = 15.0f;

    // Bredd (0 = mono, 1 = mycket brett) och dynamik (0 = dynamiskt, 1 = hårt komprimerat)
    float width = 0.0f, compression = 0.0f;

    // Underlag (för förklaringar och tester)
    float sideToMid = 0.0f, correlation = 1.0f, loudnessRangeDb = 0.0f;
    float coherence = 1.0f;     // vänster/höger, 300 Hz – 6 kHz (1 = samma signal)
    int decaySegments = 0;      // antal fria avklingningar (mono)
    enum class RoomMeasure { Stereo, MonoDecay, Default };
    RoomMeasure roomMeasure = RoomMeasure::Default;   // hur rummet mättes

    // Korta förklaringar på svenska (UTF-8), en per område.
    std::string toneText, roomText, widthText, dynamicsText;
};

// left/right: samples vid sampleRate (right får vara nullptr för mono).
// progress anropas med 0..1; returnera false för att avbryta (då blir valid = false).
SongAnalysis analyseSong(const float* left, const float* right, int numSamples, double sampleRate,
                         const std::function<bool(float)>& progress = {});

} // namespace gfx
