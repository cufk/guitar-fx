#pragma once
// Låtanalys: läser en inspelad låt och föreslår inställningar för ett preset
// som får gitarren att låta mer som i låten. Ren signalanalys – ingen nedladdning,
// inget nätverk, fungerar likadant på Mac, iPad och Raspberry Pi.
//
//  Klang    medelspektrum jämfört med en rak lutning (som de flesta mixar följer):
//           avvikelser i bas / mitt / diskant blir EQ-förslag (halva avvikelsen, ±6 dB).
//  Rum      hur snabbt nivån faller efter anslag (efterklang) och hur mycket energi som
//           ligger kvar mellan anslagen (mix). Tidsavståndet till tidig energi -> predelay.
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

    // Klang
    float lowCutHz = 80.0f;
    float lowGainDb = 0.0f, midGainDb = 0.0f, midHz = 800.0f, highGainDb = 0.0f;

    // Rum
    float reverbDecaySec = 1.5f, reverbMix = 0.2f, reverbPredelayMs = 15.0f;

    // Bredd (0 = mono, 1 = mycket brett) och dynamik (0 = dynamiskt, 1 = hårt komprimerat)
    float width = 0.0f, compression = 0.0f;

    // Underlag (för förklaringar och tester)
    float sideToMid = 0.0f, correlation = 1.0f, loudnessRangeDb = 0.0f, decayDbPerSec = 0.0f, floorToPeakDb = 0.0f;

    // Korta förklaringar på svenska (UTF-8), en per område.
    std::string toneText, roomText, widthText, dynamicsText;
};

// left/right: samples vid sampleRate (right får vara nullptr för mono).
// progress anropas med 0..1; returnera false för att avbryta (då blir valid = false).
SongAnalysis analyseSong(const float* left, const float* right, int numSamples, double sampleRate,
                         const std::function<bool(float)>& progress = {});

} // namespace gfx
