#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
gfx::ParamSpec P(const char* id, const char* label, const char* unit, gfx::Parameter& p,
                 float step, int decimals, bool log = false, std::vector<std::string> choices = {}) {
    return { id, label, unit, &p, step, decimals, log, std::move(choices), p.get() };
}
} // namespace

GuitarFXProcessor::GuitarFXProcessor()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",  juce::AudioChannelSet::stereo(), true)   // Scarlett in 1 + 2
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)) {
    // Kedjan byggs en gång vid start (EffectChain::add är inte realtidssäker).
    // Ordning: EQ -> kropp -> sustain -> distortion -> kabinett -> rundgångsskydd -> dubblare ->
    // chorus -> flanger -> tremolo -> widener -> delay -> reverb -> volym.
    // Rundgångsskyddet sitter efter alla steg som höjer nivån och före modulationen
    // (som annars smetar ut topparna det letar efter).
    auto& chain = engine_.chain();
    auto add = [&chain](auto effect, auto*& ptr) { ptr = effect.get(); return chain.add(std::move(effect), false); };
    const int eqSlot   = add(std::make_unique<gfx::Eq>(),         eq_);
    const int bodySlot = add(std::make_unique<gfx::Body>(),       body_);
    const int susSlot  = add(std::make_unique<gfx::Sustain>(),    sustain_);
    const int distSlot = add(std::make_unique<gfx::Distortion>(), distortion_);
    const int cabSlot  = add(std::make_unique<gfx::Cabinet>(),    cabinet_);
    const int fbSlot   = add(std::make_unique<gfx::FeedbackSuppressor>(), feedback_);
    const int dblSlot  = add(std::make_unique<gfx::Doubler>(),    doubler_);
    const int chSlot   = add(std::make_unique<gfx::Chorus>(),     chorus_);
    const int flSlot   = add(std::make_unique<gfx::Flanger>(),    flanger_);
    const int trSlot   = add(std::make_unique<gfx::Tremolo>(),    tremolo_);
    const int widSlot  = add(std::make_unique<gfx::Widener>(),    widener_);
    const int dlSlot   = add(std::make_unique<gfx::Delay>(),      delay_);
    const int rvSlot   = add(std::make_unique<gfx::Reverb>(),     reverb_);
    const int volSlot  = add(std::make_unique<gfx::Volume>(),     volume_);

    // Färger i samma anda som StemSplitters spårfärger.
    rig_.addEffect({ "eq", "EQ", 0xff29ad9e, eqSlot, {
        P("lowCut", "Lågskärning", "Hz", eq_->lowCutHz, 1, 0, true),
        P("low",    "Bas",         "dB", eq_->lowGainDb, 0.1f, 1),
        P("midHz",  "Mitt frekv.", "Hz", eq_->midFreqHz, 1, 0, true),
        P("mid",    "Mitt",        "dB", eq_->midGainDb, 0.1f, 1),
        P("high",   "Diskant",     "dB", eq_->highGainDb, 0.1f, 1),
        P("notchHz","Notch frekv.","Hz", eq_->notchHz, 1, 0, true),
        P("notch",  "Notch djup",  "dB", eq_->notchDepthDb, 0.5f, 1) }, {}, {} });
    rig_.addEffect({ "sustain", "Sustain", 0xffe0773a, susSlot, {
        P("amount",  "Sustain", "",   sustain_->amount, 0.01f, 2),
        P("attack",  "Anslag",  "ms", sustain_->attackMs, 0.1f, 0, true),
        P("release", "Släpp",   "ms", sustain_->releaseMs, 1, 0, true),
        P("level",   "Nivå",    "dB", sustain_->levelDb, 0.1f, 1),
        P("mix",     "Mix",     "",   sustain_->mix, 0.01f, 2) },
        [this] { return sustain_->gainReductionDb(); }, {} });
    rig_.addEffect({ "chorus", "Chorus", 0xff8c6bf2, chSlot, {
        P("rate",  "Hastighet", "Hz", chorus_->rateHz, 0.01f, 2, true),
        P("depth", "Djup",      "",   chorus_->depth, 0.01f, 2),
        P("mix",   "Mix",       "",   chorus_->mix, 0.01f, 2) }, {}, {} });
    rig_.addEffect({ "flanger", "Flanger", 0xfff5a623, flSlot, {
        P("rate",     "Hastighet",    "Hz", flanger_->rateHz, 0.01f, 2, true),
        P("depth",    "Djup",         "",   flanger_->depth, 0.01f, 2),
        P("feedback", "Återkoppling", "",   flanger_->feedback, 0.01f, 2),
        P("mix",      "Mix",          "",   flanger_->mix, 0.01f, 2) }, {}, {} });
    rig_.addEffect({ "widener", "Widener", 0xff2bb3d9, widSlot, {
        P("width",  "Bredd",       "",   widener_->width, 0.01f, 2),
        P("time",   "Tid",         "ms", widener_->timeMs, 0.1f, 1),
        P("lowCut", "Bas i mitten","Hz", widener_->lowCutHz, 1, 0, true) }, {}, {} });
    rig_.addEffect({ "reverb", "Reverb", 0xff3d8ff0, rvSlot, {
        P("decay",    "Efterklang",  "s",  reverb_->decaySec, 0.01f, 2, true),
        P("size",     "Storlek",     "",   reverb_->size, 0.01f, 2),
        P("predelay", "Predelay",    "ms", reverb_->predelayMs, 0.1f, 0),
        P("damp",     "Dämpning",    "Hz", reverb_->dampHz, 1, 0, true),
        P("lowCut",   "Lågskärning", "Hz", reverb_->lowCutHz, 1, 0, true),
        P("mix",      "Mix",         "",   reverb_->mix, 0.01f, 2) }, {}, {} });
    rig_.addEffect({ "distortion", "Distortion", 0xffd9467a, distSlot, {
        P("mode",  "Typ",   "",   distortion_->mode, 1, 0, false, { "Overdrive", "Distortion", "Fuzz" }),
        P("drive", "Drive", "dB", distortion_->driveDb, 0.1f, 1),
        P("tight", "Tight", "Hz", distortion_->tightHz, 1, 0, true),
        P("tone",  "Ton",   "",   distortion_->tone, 0.01f, 2),
        P("level", "Nivå",  "dB", distortion_->levelDb, 0.1f, 1) }, {}, {} });
    rig_.addEffect({ "cabinet", "Kabinett", 0xff9a7b5f, cabSlot, {
        P("level", "Nivå", "dB", cabinet_->levelDb, 0.1f, 1) }, {}, {} });
    rig_.addEffect({ "body", "Kropp", 0xffb08a5a, bodySlot, {
        P("type",   "Typ",   "",   body_->type, 1, 0, false, { "Dreadnought", "Jumbo", "Auditorium", "Parlor" }),
        P("amount", "Mängd", "",   body_->amount, 0.01f, 2),
        P("air",    "Luft",  "dB", body_->airDb, 0.1f, 1) }, {}, "Kroppsklang" });
    rig_.addEffect({ "feedback", "Rundgång", 0xffd84a3c, fbSlot, {
        P("sensitivity", "Känslighet", "",   feedback_->sensitivity, 0.01f, 2),
        P("maxFilters",  "Max filter", "",   feedback_->maxFilters, 1, 0),
        P("maxDepth",    "Max djup",   "dB", feedback_->maxDepthDb, 0.5f, 1) },
        [this] { return static_cast<float>(feedback_->activeFilters()); }, "Rundgångsskydd" });
    rig_.addEffect({ "doubler", "Dubblare", 0xff6c8cf2, dblSlot, {
        P("doubling", "Dubbling", "",   doubler_->doubling, 0.01f, 2),
        P("octave",   "Oktav",    "",   doubler_->octave, 0.01f, 2),
        P("time",     "Tid",      "ms", doubler_->timeMs, 0.1f, 1),
        P("width",    "Bredd",    "",   doubler_->width, 0.01f, 2) }, {}, "Dubblare och oktav" });
    rig_.addEffect({ "tremolo", "Tremolo", 0xffc45ab3, trSlot, {
        P("rate",   "Hastighet", "Hz", tremolo_->rateHz, 0.01f, 2, true),
        P("depth",  "Djup",      "",   tremolo_->depth, 0.01f, 2),
        P("shape",  "Form",      "",   tremolo_->shape, 0.01f, 2),
        P("stereo", "Stereo",    "",   tremolo_->stereo, 0.01f, 2) }, {}, {} });
    rig_.addEffect({ "delay", "Delay", 0xff3fb37f, dlSlot, {
        P("tempo",    "Tempo",        "BPM", delay_->tempoBpm, 1, 0),
        P("division", "Indelning",    "",    delay_->division, 1, 0, false, { "1/4", "1/8 punkt.", "1/8", "1/16" }),
        P("feedback", "Återkoppling", "",    delay_->feedback, 0.01f, 2),
        P("damp",     "Dämpning",     "Hz",  delay_->dampHz, 1, 0, true),
        P("mix",      "Mix",          "",    delay_->mix, 0.01f, 2) }, {}, {} });
    rig_.addEffect({ "volume", "Volym", 0xff8a8a8a, volSlot, {
        P("level", "Nivå",  "dB", volume_->levelDb, 0.1f, 1),
        P("boost", "Boost", "dB", volume_->boostDb, 0.5f, 1) }, {}, {} });

    createFactoryPresets();
    startTimerHz(50);
}

GuitarFXProcessor::~GuitarFXProcessor() { stopTimer(); }

void GuitarFXProcessor::createFactoryPresets() {
    // Grundljud: EQ på (lätt lågskärning) och ett diskret rum som alltid ligger på.
    eq_->lowCutHz.set(80.0f);
    reverb_->decaySec.set(1.2f);
    reverb_->mix.set(0.15f);
    engine_.chain().setEnabled(rig_.findEffect("eq")->slot, true);
    engine_.chain().setEnabled(rig_.findEffect("reverb")->slot, true);
    // Säkerhet och nivå ligger alltid på i grundljudet. Kroppsklang är för den inbyggda
    // pickupen och slås på av den som behöver den.
    engine_.chain().setEnabled(rig_.findEffect("feedback")->slot, true);
    engine_.chain().setEnabled(rig_.findEffect("volume")->slot, true);
    rig_.initBaseFromLive();

    // Exempel att utgå från – byt namn, ändra eller ta bort.
    auto make = [this](const char* name, auto setup) {
        const int i = rig_.addPreset(name);
        rig_.activate(i);
        setup();
        rig_.syncFromLive();
    };
    make("Stor hall", [this] {
        rig_.setIncludedInActive("reverb", true);
        reverb_->decaySec.set(4.5f); reverb_->size.set(0.9f); reverb_->predelayMs.set(30.0f); reverb_->mix.set(0.35f);
    });
    make("Chorus", [this] {
        rig_.setIncludedInActive("chorus", true);
        chorus_->rateHz.set(0.5f); chorus_->depth.set(0.4f); chorus_->mix.set(0.35f);
    });
    make("Flanger-svep", [this] {
        rig_.setIncludedInActive("flanger", true);
        flanger_->rateHz.set(0.15f); flanger_->feedback.set(0.4f); flanger_->mix.set(0.35f);
    });
    rig_.activate(-1);
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

void GuitarFXProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::AudioProcessLoadMeasurer::ScopedTimer timer(loadMeasurer_, buffer.getNumSamples());

    // Fotswitch/Bluetooth-MIDI: Program Change 0 = preset 1, 1 = preset 2 osv. (samma igen
    // eller nummer utan preset = grundljud). CC 102/103 = föregående/nästa, CC 104 = grundljud,
    // CC 105 = tysta ut (stämning) på/av, CC 106 = boost på/av, CC 107 = tap tempo.
    for (const auto meta : midi) {
        const auto m = meta.getMessage();
        midiEvents_.fetch_add(1, std::memory_order_relaxed);
        if (m.isProgramChange()) {
            pendingCommand_.store(m.getProgramChangeNumber(), std::memory_order_release);
            lastMidi_.store((1 << 16) | (m.getProgramChangeNumber() << 8), std::memory_order_relaxed);
        } else if (m.isController() && m.getControllerValue() >= 64) {
            const int cc = m.getControllerNumber();
            if (cc == 102) pendingCommand_.store(kCmdPrev, std::memory_order_release);
            if (cc == 103) pendingCommand_.store(kCmdNext, std::memory_order_release);
            if (cc == 104) pendingCommand_.store(kCmdBase, std::memory_order_release);
            if (cc == 105) engine_.tuner().muteOutput.store(!engine_.tuner().muteOutput.load(std::memory_order_relaxed), std::memory_order_relaxed);
            if (cc == 106) volume_->setBoost(!volume_->boostOn());
            if (cc == 107) {   // tidpunkten tas här, så att timerns ryck inte påverkar tempot
                midiTapMs_.store(juce::Time::getMillisecondCounterHiRes(), std::memory_order_relaxed);
                midiTaps_.fetch_add(1, std::memory_order_release);
            }
            lastMidi_.store((2 << 16) | (cc << 8) | m.getControllerValue(), std::memory_order_relaxed);
        }
    }

    // JUCE ger in- och utsignal i samma buffert. Engine läser ingång 1/2 och
    // skriver resultatet till alla kanaler.
    const int numCh = juce::jmin(buffer.getNumChannels(), gfx::kMaxChannels);
    gfx::AudioBufferView view { buffer.getArrayOfWritePointers(), numCh, buffer.getNumSamples() };
    engine_.process(view);
}

void GuitarFXProcessor::timerCallback() {
    const int cmd = pendingCommand_.exchange(kCmdNone, std::memory_order_acq_rel);
    if (cmd == kCmdNext) rig_.selectNext();
    else if (cmd == kCmdPrev) rig_.selectPrevious();
    else if (cmd == kCmdBase) rig_.activate(-1);
    else if (cmd >= 0) rig_.select(cmd < static_cast<int>(rig_.presets().size()) ? cmd : -1);

    if (const int taps = midiTaps_.load(std::memory_order_acquire); taps != handledMidiTaps_) {
        handledMidiTaps_ = taps;
        const double ms = midiTapMs_.load(std::memory_order_relaxed);
        lastTapMs_.store(ms, std::memory_order_relaxed);
        if (const float bpm = tap_.tap(ms); bpm > 0.0f) delay_->tempoBpm.set(std::round(bpm));
    }

    // Ändringar från GUI:t (rattar, på/av) skrivs till grundljud eller aktivt preset.
    rig_.syncFromLive();
    cabinet_->collectGarbage();
}

int GuitarFXProcessor::createPresetFromAnalysis(const gfx::GuitarFit& f, const juce::String& name) {
    const int i = rig_.addPreset(name.toStdString());
    rig_.activate(i);

    // Plats i bandet: lågskärning, bas, mittband (gröt-sänkning eller närvaro) och diskant.
    // Notch lämnas orörd (den är till för rundgång).
    rig_.setIncludedInActive("eq", true);
    eq_->lowCutHz.set(f.lowCutHz);
    eq_->lowGainDb.set(f.lowShelfDb);
    eq_->midFreqHz.set(f.midHz);
    eq_->midGainDb.set(f.midDb);
    eq_->highGainDb.set(f.highDb);

    // Rum: lite torrare än låten, predelay efter tempot.
    rig_.setIncludedInActive("reverb", true);
    reverb_->decaySec.set(f.reverbDecaySec);
    reverb_->mix.set(f.reverbMix);
    reverb_->predelayMs.set(f.reverbPredelayMs);
    reverb_->size.set(juce::jlimit(0.3f, 1.0f, f.reverbDecaySec / 3.0f));

    // Dynamik: jämnare gitarr i en tät låt (parallellt, så att anslaget behålls).
    if (f.sustain) {
        rig_.setIncludedInActive("sustain", true);
        sustain_->amount.set(f.sustainAmount);
        sustain_->mix.set(f.sustainMix);
    }
    // Tempo: delayn får låtens tempo men är avstängd – slå på den om låten har eko.
    if (f.delayBpm > 0.0f) {
        rig_.setIncludedInActive("delay", true);
        delay_->tempoBpm.set(f.delayBpm);
        delay_->division.set(static_cast<float>(f.delayDivision));
        engine_.chain().setEnabled(rig_.findEffect("delay")->slot, false);
    }
    // Ingen widener: gitarren ligger smal i bandet.
    rig_.syncFromLive();
    return i;
}

void GuitarFXProcessor::tapTempo() {
    const double ms = juce::Time::getMillisecondCounterHiRes();
    lastTapMs_.store(ms, std::memory_order_relaxed);
    if (const float bpm = tap_.tap(ms); bpm > 0.0f) delay_->tempoBpm.set(std::round(bpm));
}

juce::String GuitarFXProcessor::lastMidiDescription() const {
    const int v = lastMidi_.load(std::memory_order_relaxed);
    if (v < 0) return {};
    const int type = v >> 16, num = (v >> 8) & 0xff;
    return type == 1 ? "PC " + juce::String(num) : "CC " + juce::String(num);
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

// ---------------------------------------------------------------------------
// JSON för grundljud + presets
// ---------------------------------------------------------------------------
namespace {
juce::var stateToJson(const gfx::EffectState& s) {
    auto* o = new juce::DynamicObject();
    o->setProperty("on", s.enabled);
    auto* vals = new juce::DynamicObject();
    for (const auto& [k, v] : s.values) vals->setProperty(juce::Identifier(k), v);
    o->setProperty("values", juce::var(vals));
    return juce::var(o);
}

juce::var snapshotToJson(const gfx::Snapshot& snap) {
    auto* o = new juce::DynamicObject();
    for (const auto& [id, s] : snap) o->setProperty(juce::Identifier(id), stateToJson(s));
    return juce::var(o);
}

gfx::Snapshot snapshotFromJson(const juce::var& v) {
    gfx::Snapshot snap;
    if (auto* o = v.getDynamicObject()) {
        for (const auto& prop : o->getProperties()) {
            gfx::EffectState s;
            s.enabled = static_cast<bool>(prop.value.getProperty("on", false));
            if (auto* vals = prop.value.getProperty("values", {}).getDynamicObject())
                for (const auto& pv : vals->getProperties())
                    s.values[pv.name.toString().toStdString()] = static_cast<float>(static_cast<double>(pv.value));
            snap[prop.name.toString().toStdString()] = s;
        }
    }
    return snap;
}
} // namespace

juce::var GuitarFXProcessor::rigToJson() const {
    auto* root = new juce::DynamicObject();
    root->setProperty("format", "guitarfx-rig");
    root->setProperty("version", 1);
    root->setProperty("base", snapshotToJson(rig_.base()));
    juce::Array<juce::var> presets;
    for (const auto& p : rig_.presets()) {
        auto* o = new juce::DynamicObject();
        o->setProperty("name", juce::String::fromUTF8(p.name.c_str()));
        o->setProperty("effects", snapshotToJson(p.overrides));
        presets.add(juce::var(o));
    }
    root->setProperty("presets", presets);
    root->setProperty("active", rig_.activePreset());
    return juce::var(root);
}

bool GuitarFXProcessor::rigFromJson(const juce::var& json) {
    if (json.getProperty("format", "").toString() != "guitarfx-rig") return false;
    std::vector<gfx::Preset> presets;
    if (auto* arr = json.getProperty("presets", {}).getArray())
        for (const auto& p : *arr)
            presets.push_back({ p.getProperty("name", "").toString().toStdString(),
                                snapshotFromJson(p.getProperty("effects", {})) });
    rig_.load(snapshotFromJson(json.getProperty("base", {})), presets,
              static_cast<int>(json.getProperty("active", -1)));
    return true;
}

// State: ingång/nivåer (globalt), grundljud + presets (JSON) och IR-fil.
void GuitarFXProcessor::getStateInformation(juce::MemoryBlock& destData) {
    juce::ValueTree state("GuitarFX");
    state.setProperty("version", 5, nullptr);
    auto& in = engine_.input();
    state.setProperty("inputMode",    in.mode.load(), nullptr);
    state.setProperty("inputBalance", in.balance.get(), nullptr);
    state.setProperty("inputAlignMs", in.alignMs.get(), nullptr);
    state.setProperty("inputInvert1", in.invert1.load(), nullptr);
    state.setProperty("inputInvert2", in.invert2.load(), nullptr);
    state.setProperty("inputGainDb",  engine_.inputGain().gainDb.get(), nullptr);
    state.setProperty("outputGainDb", engine_.outputGain().gainDb.get(), nullptr);
    state.setProperty("monoOutput",   engine_.monoOutput.load(), nullptr);
    state.setProperty("appearance",   appearance.load(), nullptr);
    state.setProperty("tuning",       tuning.load(), nullptr);
    state.setProperty("referenceA4",  referenceA4.load(), nullptr);
    state.setProperty("screensaver",  screensaver.load(), nullptr);
    state.setProperty("screensaverMinutes", screensaverMinutes.load(), nullptr);
    state.setProperty("cabIr",        irFile_.getFullPathName(), nullptr);
    state.setProperty("rig",          juce::JSON::toString(rigToJson(), true), nullptr);
    juce::MemoryOutputStream stream(destData, false);
    state.writeToStream(stream);
}

void GuitarFXProcessor::setStateInformation(const void* data, int sizeInBytes) {
    const auto state = juce::ValueTree::readFromData(data, static_cast<size_t>(sizeInBytes));
    if (!state.isValid()) return;
    auto f = [&](const char* key, float def) { return static_cast<float>(state.getProperty(key, def)); };

    auto& in = engine_.input();
    in.mode.store(juce::jlimit(0, 2, static_cast<int>(state.getProperty("inputMode", state.getProperty("inputChannel", 0)))));
    in.balance.set(f("inputBalance", 0.5f));
    in.alignMs.set(f("inputAlignMs", 0.0f));
    in.invert1.store(static_cast<bool>(state.getProperty("inputInvert1", false)));
    in.invert2.store(static_cast<bool>(state.getProperty("inputInvert2", false)));
    engine_.inputGain().gainDb.set(f("inputGainDb", 0.0f));
    engine_.outputGain().gainDb.set(f("outputGainDb", 0.0f));
    engine_.monoOutput.store(static_cast<bool>(state.getProperty("monoOutput", false)));
    appearance.store(juce::jlimit(0, 2, static_cast<int>(state.getProperty("appearance", 0))));
    tuning.store(juce::jmax(0, static_cast<int>(state.getProperty("tuning", 0))));
    referenceA4.store(juce::jlimit(420.0f, 460.0f, static_cast<float>(state.getProperty("referenceA4", 440.0f))));
    screensaver.store(juce::jlimit(0, 4, static_cast<int>(state.getProperty("screensaver", 0))));
    screensaverMinutes.store(juce::jlimit(1, 60, static_cast<int>(state.getProperty("screensaverMinutes", 5))));

    const juce::String ir = state.getProperty("cabIr", "").toString();
    if (ir.isNotEmpty() && juce::File(ir).existsAsFile()) loadImpulseResponse(juce::File(ir));
    else useBuiltInImpulseResponse();

    // Äldre state (före presets) saknar "rig": då behålls fabrikspresets.
    const auto rigJson = juce::JSON::parse(state.getProperty("rig", "").toString());
    if (rigJson.isObject()) rigFromJson(rigJson);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new GuitarFXProcessor();
}
