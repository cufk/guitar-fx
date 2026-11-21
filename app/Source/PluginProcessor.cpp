#include "PluginProcessor.h"
#include "PluginEditor.h"

GuitarFXProcessor::GuitarFXProcessor()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",  juce::AudioChannelSet::stereo(), true)   // Scarlett in 1 (+2)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)) {
    // Kedjan byggs en gång vid start (EffectChain::add är inte realtidssäker).
    // Ordning: distortion -> kabinett -> chorus -> flanger -> reverb. Allt av från start.
    auto add = [this](auto effect, auto*& ptr, int& slot, const char* key) {
        ptr = effect.get();
        slot = engine_.chain().add(std::move(effect), false);
        slots_.push_back({ key, slot });
    };
    add(std::make_unique<gfx::Distortion>(), distortion_, distSlot_,    "distOn");
    add(std::make_unique<gfx::Cabinet>(),    cabinet_,    cabSlot_,     "cabOn");
    add(std::make_unique<gfx::Chorus>(),     chorus_,     chorusSlot_,  "chorusOn");
    add(std::make_unique<gfx::Flanger>(),    flanger_,    flangerSlot_, "flangerOn");
    add(std::make_unique<gfx::Reverb>(),     reverb_,     reverbSlot_,  "reverbOn");

    auto& in = engine_.input();
    registerParam("inputGainDb",  engine_.inputGain().gainDb);
    registerParam("outputGainDb", engine_.outputGain().gainDb);
    registerParam("inputBalance", in.balance);
    registerParam("inputAlignMs", in.alignMs);
    registerParam("distDrive",    distortion_->driveDb);
    registerParam("distTone",     distortion_->tone);
    registerParam("distLevel",    distortion_->levelDb);
    registerParam("distMode",     distortion_->mode);
    registerParam("distTight",    distortion_->tightHz);
    registerParam("cabLevel",     cabinet_->levelDb);
    registerParam("chorusRate",   chorus_->rateHz);
    registerParam("chorusDepth",  chorus_->depth);
    registerParam("chorusMix",    chorus_->mix);
    registerParam("flangerRate",  flanger_->rateHz);
    registerParam("flangerDepth", flanger_->depth);
    registerParam("flangerFeedback", flanger_->feedback);
    registerParam("flangerMix",   flanger_->mix);
    registerParam("reverbDecay",  reverb_->decaySec);
    registerParam("reverbSize",   reverb_->size);
    registerParam("reverbPredelay", reverb_->predelayMs);
    registerParam("reverbDamp",   reverb_->dampHz);
    registerParam("reverbLowCut", reverb_->lowCutHz);
    registerParam("reverbMix",    reverb_->mix);
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

// State: alla registrerade parametrar, effekternas på/av, ingångsval och IR-fil.
// Ersätts av preset-systemet (JSON) senare.
void GuitarFXProcessor::getStateInformation(juce::MemoryBlock& destData) {
    juce::ValueTree state("GuitarFX");
    state.setProperty("version", 4, nullptr);
    for (const auto& e : params_) state.setProperty(e.key, e.param->get(), nullptr);
    for (const auto& e : slots_)  state.setProperty(e.key, engine_.chain().isEnabled(e.slot), nullptr);

    auto& in = engine_.input();
    state.setProperty("inputMode",    in.mode.load(), nullptr);
    state.setProperty("inputInvert1", in.invert1.load(), nullptr);
    state.setProperty("inputInvert2", in.invert2.load(), nullptr);
    state.setProperty("cabIr",        irFile_.getFullPathName(), nullptr);

    juce::MemoryOutputStream stream(destData, false);
    state.writeToStream(stream);
}

void GuitarFXProcessor::setStateInformation(const void* data, int sizeInBytes) {
    const auto state = juce::ValueTree::readFromData(data, static_cast<size_t>(sizeInBytes));
    if (!state.isValid()) return;

    // Saknade nycklar (äldre state) får standardvärdet.
    for (const auto& e : params_)
        e.param->set(static_cast<float>(state.getProperty(e.key, e.defaultValue)));
    for (const auto& e : slots_)
        engine_.chain().setEnabled(e.slot, static_cast<bool>(state.getProperty(e.key, false)));

    auto& in = engine_.input();
    // Äldre state (v2) hade bara "inputChannel" (0/1) – motsvarar läge Ingång 1/2.
    in.mode.store(juce::jlimit(0, 2, static_cast<int>(state.getProperty("inputMode", state.getProperty("inputChannel", 0)))));
    in.invert1.store(static_cast<bool>(state.getProperty("inputInvert1", false)));
    in.invert2.store(static_cast<bool>(state.getProperty("inputInvert2", false)));

    const juce::String ir = state.getProperty("cabIr", "").toString();
    if (ir.isNotEmpty() && juce::File(ir).existsAsFile()) loadImpulseResponse(juce::File(ir));
    else useBuiltInImpulseResponse();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new GuitarFXProcessor();
}
