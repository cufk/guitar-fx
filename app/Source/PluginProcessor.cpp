#include "PluginProcessor.h"
#include "PluginEditor.h"

GuitarFXProcessor::GuitarFXProcessor()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",  juce::AudioChannelSet::stereo(), true)   // Scarlett in 1 (+2)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)) {
    // Kedjan byggs en gång vid start (EffectChain::add är inte realtidssäker).
    auto dist = std::make_unique<gfx::Distortion>();
    auto cab  = std::make_unique<gfx::Cabinet>();
    distortion_ = dist.get();
    cabinet_    = cab.get();
    distSlot_ = engine_.chain().add(std::move(dist), false);
    cabSlot_  = engine_.chain().add(std::move(cab),  false);
}

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

juce::String GuitarFXProcessor::loadImpulseResponse(const juce::File& file) {
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (!reader) return juce::String::fromUTF8("Kunde inte läsa ") + file.getFileName();

    const auto maxSamples = static_cast<juce::int64>(reader->sampleRate * 2.0); // läs högst 2 s
    const int n = static_cast<int>(juce::jmin(reader->lengthInSamples, maxSamples));
    if (n <= 0) return "Tom fil: " + file.getFileName();

    juce::AudioBuffer<float> buf(static_cast<int>(reader->numChannels), n);
    reader->read(&buf, 0, n, 0, true, true);
    if (!cabinet_->loadImpulseResponse(buf.getReadPointer(0), n, reader->sampleRate,
                                       file.getFileNameWithoutExtension().toStdString()))
        return "Ogiltig IR (tyst eller trasig): " + file.getFileName();
    irFile_ = file;
    return {};
}

void GuitarFXProcessor::useBuiltInImpulseResponse() {
    cabinet_->useBuiltInImpulseResponse();
    irFile_ = juce::File();
}

// Enkel state: nivåer, effekter och IR-fil. Ersätts av preset-systemet (JSON) senare.
void GuitarFXProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto& d = *distortion_;
    auto& c = *cabinet_;
    juce::ValueTree state("GuitarFX");
    state.setProperty("version", 3, nullptr);
    state.setProperty("inputGainDb",  engine_.inputGain().gainDb.get(), nullptr);
    state.setProperty("outputGainDb", engine_.outputGain().gainDb.get(), nullptr);
    auto& in = engine_.input();
    state.setProperty("inputMode",    in.mode.load(), nullptr);
    state.setProperty("inputBalance", in.balance.get(), nullptr);
    state.setProperty("inputAlignMs", in.alignMs.get(), nullptr);
    state.setProperty("inputInvert1", in.invert1.load(), nullptr);
    state.setProperty("inputInvert2", in.invert2.load(), nullptr);

    state.setProperty("distOn",    engine_.chain().isEnabled(distSlot_), nullptr);
    state.setProperty("distDrive", d.driveDb.get(), nullptr);
    state.setProperty("distTone",  d.tone.get(), nullptr);
    state.setProperty("distLevel", d.levelDb.get(), nullptr);
    state.setProperty("distMode",  d.mode.get(), nullptr);
    state.setProperty("distTight", d.tightHz.get(), nullptr);

    state.setProperty("cabOn",    engine_.chain().isEnabled(cabSlot_), nullptr);
    state.setProperty("cabLevel", c.levelDb.get(), nullptr);
    state.setProperty("cabIr",    irFile_.getFullPathName(), nullptr);

    juce::MemoryOutputStream stream(destData, false);
    state.writeToStream(stream);
}

void GuitarFXProcessor::setStateInformation(const void* data, int sizeInBytes) {
    const auto state = juce::ValueTree::readFromData(data, static_cast<size_t>(sizeInBytes));
    if (!state.isValid()) return;
    auto f = [&](const char* key, float def) { return static_cast<float>(state.getProperty(key, def)); };

    engine_.inputGain().gainDb.set(f("inputGainDb", 0.0f));
    engine_.outputGain().gainDb.set(f("outputGainDb", 0.0f));
    auto& in = engine_.input();
    // Äldre state (v2) hade bara "inputChannel" (0/1) – motsvarar läge Ingång 1/2.
    in.mode.store(juce::jlimit(0, 2, static_cast<int>(state.getProperty("inputMode", state.getProperty("inputChannel", 0)))));
    in.balance.set(f("inputBalance", 0.5f));
    in.alignMs.set(f("inputAlignMs", 0.0f));
    in.invert1.store(static_cast<bool>(state.getProperty("inputInvert1", false)));
    in.invert2.store(static_cast<bool>(state.getProperty("inputInvert2", false)));

    auto& d = *distortion_;
    engine_.chain().setEnabled(distSlot_, static_cast<bool>(state.getProperty("distOn", false)));
    d.driveDb.set(f("distDrive", 18.0f));
    d.tone.set(f("distTone", 0.5f));
    d.levelDb.set(f("distLevel", -6.0f));
    d.mode.set(f("distMode", 0.0f));
    d.tightHz.set(f("distTight", 120.0f));

    engine_.chain().setEnabled(cabSlot_, static_cast<bool>(state.getProperty("cabOn", false)));
    cabinet_->levelDb.set(f("cabLevel", 0.0f));
    const juce::String ir = state.getProperty("cabIr", "").toString();
    if (ir.isNotEmpty() && juce::File(ir).existsAsFile()) loadImpulseResponse(juce::File(ir));
    else useBuiltInImpulseResponse();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new GuitarFXProcessor();
}
