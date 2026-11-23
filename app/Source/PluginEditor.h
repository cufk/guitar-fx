#pragma once
// Stagebox – scen-GUI, monokromt:
//   sidhuvud  – appens namn, signalindikator (bara använda ingångar) och ⋯ (alla inställningar)
//   stämfält  – lågt fält längst ned, alltid synligt (Mute = T)
//   vänster   – setlist: grundljud + presets i spelordning (dra för att ordna, Föregående/Nästa)
//   höger     – "Effekter" som rutor i signalordning (svart = på) + vald effekt med lodräta reglage
// Tangenter: 1–9 = preset, 0/Esc = grundljud, ←/→ (PgUp/PgDn) = föregående/nästa, T = Mute (stämning),
// B = boost, mellanslag = tap tempo.

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
    void showInputPanel();
    void setAnalysisOpen(bool open);
    void startLatencyMeasurement();
    void exportPresets();
    void importPresets();
    juce::String latencyText(bool shortForm) const;

    GuitarFXProcessor& processor_;
    ui::LookAndFeel lnf_;
    ui::HeaderBar header_;
    ui::SetlistPanel setlist_;
    ui::InputMeter meter_;
    ui::EffectGrid grid_;
    ui::EffectPanel panel_;
    ui::TunerStrip tuner_;
    ui::AnalysisView analysisView_;
    juce::TooltipWindow tooltips_ { this, 700 };
    std::unique_ptr<juce::FileChooser> chooser_;

    uint64_t lastBlockCount_ = 0;
    int stalledTicks_ = 0, tick_ = 0, meterPillWidth_ = 0;
    bool dark_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarFXEditor)
};
