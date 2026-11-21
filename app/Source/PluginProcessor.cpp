#include "PluginProcessor.h"
#include "PluginEditor.h"

GuitarFXProcessor::GuitarFXProcessor()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",  juce::AudioChannelSet::stereo(), true)   // Scarlett in 1 (+2)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}

bool GuitarFXProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    const bool inOk  = in  == juce::AudioChannelSet::mono() || in  == juce::AudioChannelSet::stereo();
    const bool outOk = out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
    return inOk && outOk;
}

void GuitarFXProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    gfx::ProcessSpec spec;
    spec.sampleRate   = sampleRate;
    spec.maxBlockSize = juce::jmax(samplesPerBlock, 32);
    spec.numChannels  = juce::jmax(getTotalNumInputChannels(), getTotalNumOutputChannels());
    engine_.prepare(spec);
    loadMeasurer_.reset(sampleRate, samplesPerBlock);
    setLatencySamples(engine_.latencySamples());
}

void GuitarFXProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::AudioProcessLoadMeasurer::ScopedTimer timer(loadMeasurer_, buffer.getNumSamples());

    // JUCE ger in- och utsignal i samma buffert. Engine läser gitarren från
    // vald ingångskanal och skriver resultatet till alla kanaler.
    const int numCh = juce::jmin(buffer.getNumChannels(), gfx::kMaxChannels);
    gfx::AudioBufferView view { buffer.getArrayOfWritePointers(), numCh, buffer.getNumSamples() };
    engine_.process(view);
}

juce::AudioProcessorEditor* GuitarFXProcessor::createEditor() {
    return new GuitarFXEditor(*this);
}

// Enkel state: in/ut-gain och vald ingång. Ersätts av preset-systemet senare.
void GuitarFXProcessor::getStateInformation(juce::MemoryBlock& destData) {
    juce::ValueTree state("GuitarFX");
    state.setProperty("inputGainDb",  engine_.inputGain().gainDb.get(), nullptr);
    state.setProperty("outputGainDb", engine_.outputGain().gainDb.get(), nullptr);
    state.setProperty("inputChannel", engine_.inputChannel.load(), nullptr);
    juce::MemoryOutputStream stream(destData, false);
    state.writeToStream(stream);
}

void GuitarFXProcessor::setStateInformation(const void* data, int sizeInBytes) {
    const auto state = juce::ValueTree::readFromData(data, static_cast<size_t>(sizeInBytes));
    if (!state.isValid()) return;
    engine_.inputGain().gainDb.set(static_cast<float>(state.getProperty("inputGainDb", 0.0f)));
    engine_.outputGain().gainDb.set(static_cast<float>(state.getProperty("outputGainDb", 0.0f)));
    engine_.inputChannel.store(static_cast<int>(state.getProperty("inputChannel", 0)));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new GuitarFXProcessor();
}
