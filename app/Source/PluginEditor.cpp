#include "PluginEditor.h"

namespace {
const juce::Colour kBg      { 0xff15171a };
const juce::Colour kPanel   { 0xff22252a };
const juce::Colour kText    { 0xffe8e8e8 };
const juce::Colour kDim     { 0xff8a9099 };
const juce::Colour kGood    { 0xff3ecf8e };
const juce::Colour kWarn    { 0xfff5a524 };
const juce::Colour kDanger  { 0xffef4444 };
}

// ---------------------------------------------------------------------------
// ParamKnob
// ---------------------------------------------------------------------------
ParamKnob::ParamKnob(const juce::String& name, gfx::Parameter& p, double step, const juce::String& suffix,
                     double defaultValue)
    : param_(p) {
    slider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 22);
    slider_.setRange(p.min(), p.max(), step);
    slider_.setTextValueSuffix(suffix);
    slider_.setDoubleClickReturnValue(true, defaultValue);
    slider_.setValue(p.get(), juce::dontSendNotification);
    slider_.onValueChange = [this] { param_.set(static_cast<float>(slider_.getValue())); };
    addAndMakeVisible(slider_);

    label_.setText(name, juce::dontSendNotification);
    label_.setJustificationType(juce::Justification::centred);
    label_.setColour(juce::Label::textColourId, kText);
    label_.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    addAndMakeVisible(label_);
}

void ParamKnob::syncFromParameter() {
    if (!slider_.isMouseButtonDown())
        slider_.setValue(param_.get(), juce::dontSendNotification);
}

void ParamKnob::resized() {
    auto r = getLocalBounds();
    label_.setBounds(r.removeFromTop(20));
    slider_.setBounds(r);
}

// ---------------------------------------------------------------------------
// Editor
// ---------------------------------------------------------------------------
GuitarFXEditor::GuitarFXEditor(GuitarFXProcessor& p)
    : AudioProcessorEditor(&p), processor_(p),
      inputGain_ ("IN",    p.engine().inputGain().gainDb,  0.1,  " dB", 0.0),
      outputGain_("UT",    p.engine().outputGain().gainDb, 0.1,  " dB", 0.0),
      drive_     ("Drive", p.distortion().driveDb,         0.1,  " dB", 18.0),
      tight_     ("Tight", p.distortion().tightHz,         1.0,  " Hz", 120.0),
      tone_      ("Ton",   p.distortion().tone,            0.01, "",    0.5),
      distLevel_ (juce::String::fromUTF8("Nivå"),  p.distortion().levelDb,         0.1,  " dB", -6.0),
      cabLevel_  (juce::String::fromUTF8("Nivå"),  p.cabinet().levelDb,            0.1,  " dB", 0.0) {

    for (auto* k : { &inputGain_, &outputGain_, &drive_, &tight_, &tone_, &distLevel_, &cabLevel_ })
        addAndMakeVisible(k);

    inputChannel_.addItem(juce::String::fromUTF8("Ingång 1"), 1);
    inputChannel_.addItem(juce::String::fromUTF8("Ingång 2"), 2);
    inputChannel_.setSelectedId(processor_.engine().inputChannel.load() + 1, juce::dontSendNotification);
    inputChannel_.onChange = [this] {
        processor_.engine().inputChannel.store(inputChannel_.getSelectedId() - 1);
    };
    addAndMakeVisible(inputChannel_);

    latencyButton_.onClick = [this] {
        auto& probe = processor_.engine().latencyProbe();
        if (probe.isActive()) { probe.cancel(); return; }
        juce::Component::SafePointer<GuitarFXEditor> safe(this);
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::WarningIcon)
                .withTitle(juce::String::fromUTF8("Mät latens"))
                .withMessage(juce::String::fromUTF8("Koppla en kabel från utgång 1 till vald ingång (INST av, gain lågt).\n"
                             "Koppla bort förstärkare/högtalare – mätningen skickar ut klick på -12 dBFS.\n"
                             "Mätningen tar ca 4 sekunder."))
                .withButton("Starta", 1)
                .withButton("Avbryt", 0)
                .withAssociatedComponent(this),
            [safe](int result) {
                if (safe != nullptr && result == 1) safe->processor_.engine().latencyProbe().start();
            });
    };
    addAndMakeVisible(latencyButton_);
    latencyLabel_.setColour(juce::Label::textColourId, kDim);
    addAndMakeVisible(latencyLabel_);

    setupToggle(distOn_, processor_.distortionSlot());
    setupToggle(cabOn_,  processor_.cabinetSlot());

    distMode_.addItem("Overdrive", 1);
    distMode_.addItem("Distortion", 2);
    distMode_.addItem("Fuzz", 3);
    distMode_.setSelectedId(static_cast<int>(processor_.distortion().mode.get()) + 1, juce::dontSendNotification);
    distMode_.onChange = [this] { processor_.distortion().mode.set(static_cast<float>(distMode_.getSelectedId() - 1)); };
    addAndMakeVisible(distMode_);

    loadIr_.onClick = [this] { chooseImpulseResponse(); };
    builtInIr_.onClick = [this] { processor_.useBuiltInImpulseResponse(); irError_.clear(); };
    addAndMakeVisible(loadIr_);
    addAndMakeVisible(builtInIr_);
    irName_.setJustificationType(juce::Justification::centred);
    irName_.setColour(juce::Label::textColourId, kDim);
    addAndMakeVisible(irName_);

    setResizable(true, true);
    setResizeLimits(720, 440, 2400, 1600);
    setSize(1000, 560);
    startTimerHz(30);
}

GuitarFXEditor::~GuitarFXEditor() {
    processor_.engine().latencyProbe().cancel();
}

void GuitarFXEditor::setupToggle(juce::TextButton& b, int slot) {
    b.setClickingTogglesState(true);
    b.setToggleState(processor_.engine().chain().isEnabled(slot), juce::dontSendNotification);
    b.setColour(juce::TextButton::buttonColourId, kPanel.brighter(0.15f));
    b.setColour(juce::TextButton::buttonOnColourId, kGood.darker(0.3f));
    b.setColour(juce::TextButton::textColourOffId, kDim);
    b.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    b.onClick = [this, &b, slot] { processor_.engine().chain().setEnabled(slot, b.getToggleState()); };
    addAndMakeVisible(b);
}

void GuitarFXEditor::chooseImpulseResponse() {
    chooser_ = std::make_unique<juce::FileChooser>(juce::String::fromUTF8("Välj kabinett-IR"), processor_.impulseResponseFile(),
                                                   "*.wav;*.aif;*.aiff");
    chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this](const juce::FileChooser& fc) {
                              const auto file = fc.getResult();
                              if (file == juce::File()) return;
                              irError_ = processor_.loadImpulseResponse(file);
                          });
}

void GuitarFXEditor::updateLatencyLabel() {
    using S = gfx::LatencyProbe::Status;
    const auto& probe = processor_.engine().latencyProbe();
    juce::String text;
    juce::Colour colour = kDim;
    switch (probe.status()) {
        case S::Idle:         text = juce::String::fromUTF8("Latens: ej mätt"); break;
        case S::Running:      text = juce::String::fromUTF8("Mäter..."); colour = kWarn; break;
        case S::Done:         text = "Rundtur: " + juce::String(probe.resultMs(), 2) + " ms ("
                                     + juce::String(probe.resultSamples()) + " smp)"; colour = kGood; break;
        case S::NoSignal:     text = juce::String::fromUTF8("Ingen signal tillbaka – kolla kabel/gain"); colour = kDanger; break;
        case S::Inconsistent: text = "Ostabilt resultat (" + juce::String(probe.resultMs(), 2)
                                     + juce::String::fromUTF8(" ms) – mät igen"); colour = kWarn; break;
    }
    latencyLabel_.setText(text, juce::dontSendNotification);
    latencyLabel_.setColour(juce::Label::textColourId, colour);
    latencyButton_.setButtonText(probe.isActive() ? "Avbryt" : juce::String::fromUTF8("Mät latens"));
}

void GuitarFXEditor::timerCallback() {
    auto& e = processor_.engine();
    // Peak hold med fall-off för läsbara mätare.
    inPeakHold_  = juce::jmax(e.inputPeak(),  inPeakHold_  * 0.92f);
    outPeakHold_ = juce::jmax(e.outputPeak(), outPeakHold_ * 0.92f);

    // Watchdog: har ljudmotorn levererat nya block sedan förra kollen?
    const auto blocks = e.processedBlocks();
    stalledTicks_ = (blocks == lastBlockCount_) ? stalledTicks_ + 1 : 0;
    lastBlockCount_ = blocks;

    // Synka GUI med motorn (t.ex. efter att värden laddat state).
    for (auto* k : { &inputGain_, &outputGain_, &drive_, &tight_, &tone_, &distLevel_, &cabLevel_ })
        k->syncFromParameter();
    distOn_.setToggleState(e.chain().isEnabled(processor_.distortionSlot()), juce::dontSendNotification);
    cabOn_.setToggleState(e.chain().isEnabled(processor_.cabinetSlot()), juce::dontSendNotification);
    if (!distMode_.isPopupActive())
        distMode_.setSelectedId(static_cast<int>(processor_.distortion().mode.get()) + 1, juce::dontSendNotification);

    irName_.setText(irError_.isNotEmpty() ? irError_ : juce::String::fromUTF8(processor_.cabinet().impulseResponseName().c_str()),
                    juce::dontSendNotification);
    irName_.setColour(juce::Label::textColourId, irError_.isNotEmpty() ? kDanger : kDim);
    processor_.cabinet().collectGarbage();

    updateLatencyLabel();
    repaint(meterArea_.getUnion(statusArea_).expanded(4));
    if (stalledTicks_ == 16 || stalledTicks_ == 0) repaint();
}

void GuitarFXEditor::drawMeter(juce::Graphics& g, juce::Rectangle<int> r, const juce::String& label, float peak) {
    auto bar = r.withTrimmedBottom(24);
    g.setColour(kPanel);
    g.fillRoundedRectangle(bar.toFloat(), 6.0f);

    const float db = gfx::gainToDb(peak);
    const float norm = juce::jlimit(0.0f, 1.0f, juce::jmap(db, -60.0f, 0.0f, 0.0f, 1.0f));
    auto fill = bar.reduced(4).toFloat();
    fill = fill.removeFromBottom(fill.getHeight() * norm);
    g.setColour(db > -1.0f ? kDanger : db > -12.0f ? kWarn : kGood);
    g.fillRoundedRectangle(fill, 4.0f);

    g.setColour(kText);
    g.setFont(juce::FontOptions(14.0f));
    g.drawText(label, r.removeFromBottom(20), juce::Justification::centred);
}

void GuitarFXEditor::paint(juce::Graphics& g) {
    g.fillAll(kBg);

    for (auto* panel : { &levelPanel_, &distPanel_, &cabPanel_ }) {
        g.setColour(kPanel);
        g.fillRoundedRectangle(panel->toFloat(), 10.0f);
    }
    g.setColour(kDim);
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText(juce::String::fromUTF8("NIVÅ"), levelPanel_.withHeight(36), juce::Justification::centred);

    auto& e = processor_.engine();
    auto meters = meterArea_;
    const int w = (meters.getWidth() - 12) / 2;
    drawMeter(g, meters.removeFromLeft(w), "IN", inPeakHold_);
    meters.removeFromLeft(12);
    drawMeter(g, meters, "UT", outPeakHold_);

    // Statusrad.
    const double cpu = processor_.cpuLoad() * 100.0;
    const float gr = e.limiterGainReductionDb();
    const double latMs = e.spec().sampleRate > 0 ? 1000.0 * e.latencySamples() / e.spec().sampleRate : 0.0;
    juce::String text;
    text << "CPU " << juce::String(cpu, 0) << "%"
         << "   |   Limiter " << juce::String(gr, 1) << " dB"
         << "   |   Effektlatens " << e.latencySamples() << " smp (" << juce::String(latMs, 2) << " ms)"
         << juce::String::fromUTF8("   |   Återställningar ") << static_cast<int>(e.recoveryCount());

    g.setColour(cpu > 70.0 ? kDanger : cpu > 50.0 ? kWarn : kText);
    g.setFont(juce::FontOptions(15.0f));
    g.drawText(text, statusArea_, juce::Justification::centredLeft);

}

// Ritas ovanpå alla reglage så att varningen aldrig döljs.
void GuitarFXEditor::paintOverChildren(juce::Graphics& g) {
    if (stalledTicks_ > 15) { // ~0,5 s utan nya ljudblock
        g.setColour(kDanger.withAlpha(0.9f));
        g.fillRect(getLocalBounds().removeFromTop(56));
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
        g.drawText(juce::String::fromUTF8("INGET LJUD FRÅN MOTORN – kontrollera ljudkortet"),
                   getLocalBounds().removeFromTop(56), juce::Justification::centred);
    }
}

void GuitarFXEditor::resized() {
    auto area = getLocalBounds().reduced(16);
    statusArea_ = area.removeFromBottom(32);
    area.removeFromBottom(8);

    // Toppraden: ingång + latensmätning.
    auto top = area.removeFromTop(36);
    inputChannel_.setBounds(top.removeFromLeft(160));
    top.removeFromLeft(16);
    latencyButton_.setBounds(top.removeFromLeft(130));
    top.removeFromLeft(12);
    latencyLabel_.setBounds(top);
    area.removeFromTop(12);

    meterArea_ = area.removeFromRight(110);
    area.removeFromRight(12);

    const int gap = 12;
    const int levelW = juce::jmax(120, area.getWidth() / 6);
    levelPanel_ = area.removeFromLeft(levelW);
    area.removeFromLeft(gap);
    const int cabW = juce::jmax(180, (area.getWidth() - gap) * 2 / 7);
    cabPanel_ = area.removeFromRight(cabW);
    area.removeFromRight(gap);
    distPanel_ = area;

    // Nivå: IN över UT.
    {
        auto r = levelPanel_.reduced(8);
        r.removeFromTop(28);
        inputGain_.setBounds(r.removeFromTop(r.getHeight() / 2));
        outputGain_.setBounds(r);
    }
    // Distortion: på/av + läge överst, fyra rattar under.
    {
        auto r = distPanel_.reduced(10);
        auto head = r.removeFromTop(52);
        distOn_.setBounds(head.removeFromLeft(head.getWidth() / 2).reduced(0, 2));
        head.removeFromLeft(10);
        distMode_.setBounds(head.withSizeKeepingCentre(head.getWidth(), 32));
        r.removeFromTop(8);
        const int kw = r.getWidth() / 4;
        const int kh = juce::jmin(r.getHeight(), kw + 40);
        auto row = r.withSizeKeepingCentre(r.getWidth(), kh);
        for (auto* k : { &drive_, &tight_, &tone_, &distLevel_ })
            k->setBounds(row.removeFromLeft(kw));
    }
    // Kabinett: på/av, IR-knappar, namn, nivåratt.
    {
        auto r = cabPanel_.reduced(10);
        cabOn_.setBounds(r.removeFromTop(52).reduced(0, 2));
        r.removeFromTop(8);
        auto buttons = r.removeFromTop(32);
        loadIr_.setBounds(buttons.removeFromLeft(buttons.getWidth() / 2 - 4));
        buttons.removeFromLeft(8);
        builtInIr_.setBounds(buttons);
        irName_.setBounds(r.removeFromTop(28));
        cabLevel_.setBounds(r.withSizeKeepingCentre(juce::jmin(r.getWidth(), 140), juce::jmin(r.getHeight(), 180)));
    }
}
