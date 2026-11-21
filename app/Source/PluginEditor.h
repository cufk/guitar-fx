#pragma once
// Scen-GUI, version 0: stora reglage och tydliga mätare.
//  - In/ut-nivå (peak) med klippvarning
//  - Limiterns nedreglering
//  - CPU-last och antal automatiska återställningar (NaN-skydd)
//  - Varning om ljudmotorn har slutat leverera block (watchdog)

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class GuitarFXEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit GuitarFXEditor(GuitarFXProcessor&);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawMeter(juce::Graphics&, juce::Rectangle<int>, const juce::String& label, float peak);

    GuitarFXProcessor& processor_;

    juce::Slider inputGain_, outputGain_;
    juce::Label  inputLabel_, outputLabel_;
    juce::ComboBox inputChannel_;

    float inPeakHold_ = 0.0f, outPeakHold_ = 0.0f;
    uint64_t lastBlockCount_ = 0;
    int stalledTicks_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarFXEditor)
};
