#pragma once
// Scen-GUI v2 i StemSplitter-stil:
//   sidhuvud  – ingång/nivåer (popover), titel med aktivt preset, meny
//   vänster   – effekterna som färgade rader (PÅ, I PRESET, reglage)
//   höger     – presets som stora knappar (ett i taget, igen = grundljud)
//   nederst   – nivåmätare, föregående/nästa preset, CPU/limiter/MIDI
// Tangenter: 1–9 = preset, 0/Esc = grundljud, ←/→ = föregående/nästa.

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

class GuitarFXEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit GuitarFXEditor(GuitarFXProcessor&);
    ~GuitarFXEditor() override;

    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    void visibilityChanged() override;

private:
    void timerCallback() override;
    void showMoreMenu();
    void startLatencyMeasurement();
    void exportPresets();
    void importPresets();
    juce::String latencyText() const;

    GuitarFXProcessor& processor_;
    ui::LookAndFeel lnf_;
    ui::HeaderBar header_;
    ui::EffectList effects_;
    ui::PresetPanel presets_;
    ui::BottomBar bottom_;
    juce::TooltipWindow tooltips_ { this, 600 };
    std::unique_ptr<juce::FileChooser> chooser_;

    uint64_t lastBlockCount_ = 0;
    int stalledTicks_ = 0, tick_ = 0;
    bool dark_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarFXEditor)
};
