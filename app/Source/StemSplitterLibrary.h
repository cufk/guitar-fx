#pragma once
// Läser projekt från StemSplitter (användarens andra app, Demucs-separering) på Mac:
//   ~/Library/Application Support/StemSplitter/projects/<nyckel>/
//       vocals.wav  drums.wav  bass.wav  other.wav  project.json
// project.json: sourceName, createdAt (ISO 8601), model, beatGrid.bpm (valfritt).
//
// Stämmorna ger låtanalysen bättre underlag än hela mixen (se gfx::fitGuitarToStems):
// bandets plats räknas på bas + trummor + sång utan låtens egen gitarr, och rummet mäts på
// "övrigt"-stämman (kompet). Bara Mac – iPadOS låter inte appar läsa varandras filer.

#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <functional>
#include <vector>
#include "engine/GuitarFit.h"

namespace stems {

struct Project {
    juce::String key;        // mappnamnet i StemSplitter
    juce::String name;       // låtens namn (utan filändelse)
    juce::Time created;
    float bpm = 0.0f;        // från StemSplitters slaganalys, 0 = saknas
    juce::File dir;
};

// Mappen där StemSplitter sparar projekt (finns kanske inte).
juce::File projectsRoot();
// Kompletta projekt (alla fyra stämmor), nyast först. Tom lista om StemSplitter saknas.
std::vector<Project> listProjects();

struct Result {
    bool valid = false;
    gfx::GuitarFit fit;
    float seconds = 0.0f, bpm = 0.0f;
    juce::String error;
};
// Läser stämmorna (högst 6 minuter) och räknar ut gitarrens plats. Tung – kör i egen tråd.
// progress anropas med 0..1; returnera false för att avbryta.
Result analyse(const Project& p, const std::function<bool(float)>& progress = {});

} // namespace stems
