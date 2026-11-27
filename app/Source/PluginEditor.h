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
#include "ui/ScreenSaver.h"

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
    // Skärmsläckare: aktivitet (mus/beröring var som helst i appen, tangent, MIDI) nollställer
    // tiden; första klicket när den visas väcker bara.
    void showScreensaverMenu();
    void userActivity();
    struct ActivityListener final : public juce::MouseListener {
        explicit ActivityListener(GuitarFXEditor& e) : owner(e) {}
        // JUCE skickar konstgjorda musrörelser när en komponent dyker upp under pekaren –
        // bara verkliga rörelser (> 3 px) räknas som aktivitet.
        void mouseMove(const juce::MouseEvent& e) override {
            const auto p = e.getScreenPosition();
            if (p.getDistanceFrom(last) > 3) { last = p; owner.userActivity(); }
        }
        void mouseDown(const juce::MouseEvent&) override { owner.userActivity(); }
        void mouseDrag(const juce::MouseEvent&) override { owner.userActivity(); }
        void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override { owner.userActivity(); }
        GuitarFXEditor& owner;
        juce::Point<int> last;
    };
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
    ui::ScreenSaver saver_;
    ActivityListener activity_ { *this };
    double lastActivityMs_ = 0.0, saverStartedMs_ = 0.0;
    int lastMidiEvents_ = 0;
    juce::TooltipWindow tooltips_ { this, 700 };
    std::unique_ptr<juce::FileChooser> chooser_;

    uint64_t lastBlockCount_ = 0;
    int stalledTicks_ = 0, tick_ = 0, meterPillWidth_ = 0;
    bool dark_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarFXEditor)
};
