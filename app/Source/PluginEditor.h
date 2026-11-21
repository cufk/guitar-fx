#pragma once
// Scen-GUI v4, monokromt:
//   sidhuvud  – stor rubrik med aktivt läge, ingång/utgång (popover), meny
//   vänster   – ring av streck (utnivå) runt aktivt presetnummer, namn, bläddring, mätare
//   höger     – effekterna som rutor + vald effekt med runda rattar
//   nederst   – presets som piller (ett i taget, igen = grundljud)
// Tangenter: 1–9 = preset, 0/Esc = grundljud, ←/→ = föregående/nästa, T = stämapparat.

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
    void applyAppearance(bool force);
    void showMoreMenu();
    void setTunerOpen(bool open);
    void startLatencyMeasurement();
    void exportPresets();
    void importPresets();
    juce::String latencyText(bool shortForm) const;

    GuitarFXProcessor& processor_;
    ui::LookAndFeel lnf_;
    ui::HeaderBar header_;
    ui::StageView stage_;
    ui::EffectGrid grid_;
    ui::EffectPanel panel_;
    ui::PresetBar presets_;
    ui::TunerView tunerView_;
    juce::TooltipWindow tooltips_ { this, 700 };
    std::unique_ptr<juce::FileChooser> chooser_;

    uint64_t lastBlockCount_ = 0;
    int stalledTicks_ = 0, tick_ = 0;
    bool dark_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarFXEditor)
};
