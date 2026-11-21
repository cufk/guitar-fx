#pragma once
// Scen-GUI, version 1: stora reglage och tydliga mätare.
//  - In/ut-nivå (peak) med klippvarning
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

    GuitarFXProcessor& processor_;

    juce::ComboBox inputChannel_;
    juce::TextButton latencyButton_ { juce::String::fromUTF8("Mät latens") };
    juce::Label latencyLabel_;

    ParamKnob inputGain_, outputGain_;

    juce::TextButton distOn_ { "DISTORTION" };
    juce::ComboBox distMode_;
    ParamKnob drive_, tight_, tone_, distLevel_;

    juce::TextButton cabOn_ { "KABINETT" };
    juce::TextButton loadIr_ { "Ladda IR..." }, builtInIr_ { "Inbyggd" };
    juce::Label irName_;
    ParamKnob cabLevel_;

    std::unique_ptr<juce::FileChooser> chooser_;
    juce::String irError_;

    juce::Rectangle<int> levelPanel_, distPanel_, cabPanel_, meterArea_, statusArea_;

    float inPeakHold_ = 0.0f, outPeakHold_ = 0.0f;
    uint64_t lastBlockCount_ = 0;
    int stalledTicks_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarFXEditor)
};
