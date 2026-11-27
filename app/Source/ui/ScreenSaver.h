#pragma once
// Skärmsläckare: en monokrom punkteffekt över hela appen efter en stunds inaktivitet.
//
//   Strömmar  prickar som flyter i slingrande floder (flödesfält)
//   Klot      ett roterande punktmoln som böljer organiskt
//   Terräng   ett punktlandskap i perspektiv; anslag skickar vågor mot horisonten
//   Form      en sfär av punktringar som böjer sig i lober
//
// Effekterna styrs av gitarren: nivå, bas, diskant och anslag läses ur stämapparatens
// avtappning (TunerTap) på GUI-tråden – ljudtråden påverkas inte. Punkterna ackumuleras
// i en intensitetsbuffert som färgas från nästan svart till appens ljusa bläck – alltid mörk
// (som användarens referensbilder), oavsett ljust/mörkt läge. Ingen text i vyn.
// Första klicket/beröringen väcker bara (onWake) – det går inte vidare till appen.

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>
#include <vector>
#include "../PluginProcessor.h"

namespace ui {

class ScreenSaver final : public juce::Component, private juce::Timer {
public:
    enum Mode { None = 0, Streams, Blob, Terrain, Form, NumModes };
    static const char* modeName(int mode);          // "Ingen skärmsläckare", "Strömmar", …
    static const char* shortName(int mode);         // "Av", "Strömmar", …

    explicit ScreenSaver(GuitarFXProcessor& proc);
    void start(int mode);
    void stop();
    bool isRunning() const { return running_; }
    std::function<void()> onWake;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override { if (onWake) onWake(); }

private:
    void timerCallback() override;
    void analyse();
    void renderStreams(float t);
    void renderBlob(float t);
    void renderTerrain(float t);
    void renderForm(float t);
    void computeFlowField(float t);
    void toImage();
    inline void dot(float x, float y, float a, int size);

    static const juce::Colour kBackground, kInk;
    GuitarFXProcessor& proc_;
    int mode_ = Streams;
    bool running_ = false;
    double startMs_ = 0.0;
    int frame_ = 0;

    int bw_ = 0, bh_ = 0;
    std::vector<float> buf_;
    juce::Image image_;

    // Gitarren
    std::vector<float> window_;
    float lvl_ = 0.0f, low_ = 0.0f, high_ = 0.0f, slow_ = 0.0f;
    double lastOnsetMs_ = 0.0;
    int onsets_ = 0;

    // Strömmar
    struct Particle { float x, y, s; int life; };
    std::vector<Particle> particles_;
    static constexpr int kGW = 96, kGH = 66;
    std::vector<float> fieldAx_, fieldAy_, fieldBody_;

    // Klot
    std::vector<std::array<float, 4>> sphere_;

    // Terräng
    struct Wave { double ms = -1e9; float amp = 0.0f; };
    std::array<Wave, 8> waves_ {};
    int nextWave_ = 0;

    // Form
    float lobes_ = 3.0f, depth_ = 0.12f;
};

} // namespace ui
