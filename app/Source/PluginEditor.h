#pragma once
// Scen-GUI, version 1: stora reglage och tydliga mätare.
//  - Ingång: mikrofon / inbyggd / blanda, polaritet, tidsjustering (auto)
//  - In/ut-nivå (peak per ingång) med klippvarning
//  - Distortion (läge, drive, tight, ton, nivå) och kabinett (IR, nivå) med stora på/av-knappar
//  - Latensmätning (loopback-kabel ut -> in)
//  - Limiterns nedreglering, CPU-last och antal automatiska återställningar (NaN-skydd)
//  - Varning om ljudmotorn har slutat leverera block (watchdog)

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// Ratt kopplad direkt till en gfx::Parameter (atomic – säkert från GUI-tråden).
class ParamKnob final : public juce::Component {
public:
    ParamKnob(const juce::String& name, gfx::Parameter& p, double step, const juce::String& suffix,
              double defaultValue);
    void syncFromParameter();   // t.ex. efter att state laddats
    void resized() override;

private:
    gfx::Parameter& param_;
    juce::Slider slider_;
    juce::Label label_;
};

class GuitarFXEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit GuitarFXEditor(GuitarFXProcessor&);
    ~GuitarFXEditor() override;

    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawMeter(juce::Graphics&, juce::Rectangle<int>, const juce::String& label, float peak);
    void setupToggle(juce::TextButton& b, int slot);
    void chooseImpulseResponse();
    void updateLatencyLabel();
    void updateAlignStatus();

    GuitarFXProcessor& processor_;

    juce::ComboBox inputSource_;
    juce::TextButton latencyButton_ { juce::String::fromUTF8("Mät latens") };
    juce::Label latencyLabel_;

    ParamKnob inputGain_, outputGain_;

    ParamKnob balance_, align_;
    juce::TextButton invert1_ { juce::String::fromUTF8("Ø Ingång 1") }, invert2_ { juce::String::fromUTF8("Ø Ingång 2") };
    juce::TextButton autoAlign_ { "Auto-justera" };
    juce::Label alignStatus_;
    juce::String alignText_;
    juce::Colour alignColour_;

    juce::TextButton distOn_ { "DISTORTION" };
    juce::ComboBox distMode_;
    ParamKnob drive_, tight_, tone_, distLevel_;

    juce::TextButton cabOn_ { "KABINETT" };
    juce::TextButton loadIr_ { "Ladda IR..." }, builtInIr_ { "Inbyggd" };
    juce::Label irName_;
    ParamKnob cabLevel_;

    // Generisk effektpanel: på/av-knapp + rattar (chorus, flanger, reverb).
    struct FxPanel {
        juce::TextButton onButton;
        std::vector<std::unique_ptr<ParamKnob>> knobs;
        juce::Rectangle<int> bounds;
        int slot = -1;
        int knobsPerRow = 4;
    };
    FxPanel& addFxPanel(const juce::String& title, int slot, int knobsPerRow);
    void layoutFxPanel(FxPanel& p);
    std::vector<std::unique_ptr<FxPanel>> fxPanels_;

    std::unique_ptr<juce::FileChooser> chooser_;
    juce::String irError_;

    juce::Rectangle<int> inputPanel_, levelPanel_, distPanel_, cabPanel_, meterArea_, statusArea_;

    float inPeakHold_[2] { 0.0f, 0.0f }, outPeakHold_ = 0.0f;
    uint64_t lastBlockCount_ = 0;
    int stalledTicks_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarFXEditor)
};
