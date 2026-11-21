#pragma once
// JUCE-adapter: kopplar JUCE:s ljudcallback till gfx::Engine.
// All ljudlogik ligger i engine/ och dsp/ – här finns bara limkod.

#include <juce_audio_processors/juce_audio_processors.h>
#include "engine/Engine.h"

class GuitarFXProcessor final : public juce::AudioProcessor {
public:
    GuitarFXProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GuitarFX"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    gfx::Engine& engine() { return engine_; }
    double cpuLoad() const { return loadMeasurer_.getLoadAsProportion(); }

private:
    gfx::Engine engine_;
    juce::AudioProcessLoadMeasurer loadMeasurer_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarFXProcessor)
};
