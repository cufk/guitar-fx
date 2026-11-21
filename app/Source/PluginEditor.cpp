#include "PluginEditor.h"

namespace {
const juce::Colour kBg      { 0xff15171a };
const juce::Colour kPanel   { 0xff22252a };
const juce::Colour kText    { 0xffe8e8e8 };
const juce::Colour kGood    { 0xff3ecf8e };
const juce::Colour kWarn    { 0xfff5a524 };
const juce::Colour kDanger  { 0xffef4444 };
}

GuitarFXEditor::GuitarFXEditor(GuitarFXProcessor& p)
    : AudioProcessorEditor(&p), processor_(p) {

    auto setupGain = [this](juce::Slider& s, juce::Label& l, const juce::String& text, gfx::Gain& gain) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 90, 24);
        s.setRange(gain.gainDb.min(), gain.gainDb.max(), 0.1);
        s.setValue(gain.gainDb.get(), juce::dontSendNotification);
        s.setTextValueSuffix(" dB");
        s.setDoubleClickReturnValue(true, 0.0);
        s.onValueChange = [&s, &gain] { gain.gainDb.set(static_cast<float>(s.getValue())); };
        addAndMakeVisible(s);

        l.setText(text, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centred);
        l.setColour(juce::Label::textColourId, kText);
        addAndMakeVisible(l);
    };
    setupGain(inputGain_,  inputLabel_,  "IN",  processor_.engine().inputGain());
    setupGain(outputGain_, outputLabel_, "UT",  processor_.engine().outputGain());

    inputChannel_.addItem("Ingång 1", 1);
    inputChannel_.addItem("Ingång 2", 2);
    inputChannel_.setSelectedId(processor_.engine().inputChannel.load() + 1, juce::dontSendNotification);
    inputChannel_.onChange = [this] {
        processor_.engine().inputChannel.store(inputChannel_.getSelectedId() - 1);
    };
    addAndMakeVisible(inputChannel_);

    setResizable(true, true);
    setResizeLimits(480, 320, 2000, 1400);
    setSize(720, 420);
    startTimerHz(30);
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

    repaint();
}

void GuitarFXEditor::drawMeter(juce::Graphics& g, juce::Rectangle<int> r, const juce::String& label, float peak) {
    g.setColour(kPanel);
    g.fillRoundedRectangle(r.toFloat(), 6.0f);

    const float db = gfx::gainToDb(peak);
    const float norm = juce::jlimit(0.0f, 1.0f, juce::jmap(db, -60.0f, 0.0f, 0.0f, 1.0f));
    auto fill = r.reduced(4).toFloat();
    fill = fill.removeFromBottom(fill.getHeight() * norm);
    g.setColour(db > -1.0f ? kDanger : db > -12.0f ? kWarn : kGood);
    g.fillRoundedRectangle(fill, 4.0f);

    g.setColour(kText);
    g.setFont(juce::FontOptions(14.0f));
    g.drawText(label, r.withTop(r.getBottom() + 4).withHeight(20), juce::Justification::centred);
}

void GuitarFXEditor::paint(juce::Graphics& g) {
    g.fillAll(kBg);

    auto& e = processor_.engine();
    auto area = getLocalBounds().reduced(16);
    auto status = area.removeFromBottom(36);
    auto meters = area.removeFromRight(140).withTrimmedBottom(24);
    const int w = (meters.getWidth() - 12) / 2;
    drawMeter(g, meters.removeFromLeft(w), "IN", inPeakHold_);
    meters.removeFromLeft(12);
    drawMeter(g, meters, "UT", outPeakHold_);

    // Statusrad.
    const double cpu = processor_.cpuLoad() * 100.0;
    const float gr = e.limiterGainReductionDb();
    juce::String text;
    text << "CPU " << juce::String(cpu, 0) << "%"
         << "   |   Limiter " << juce::String(gr, 1) << " dB"
         << "   |   Latens " << e.latencySamples() << " smp"
         << "   |   Återställningar " << static_cast<int>(e.recoveryCount());

    g.setColour(cpu > 70.0 ? kDanger : cpu > 50.0 ? kWarn : kText);
    g.setFont(juce::FontOptions(15.0f));
    g.drawText(text, status, juce::Justification::centredLeft);

    if (stalledTicks_ > 15) { // ~0,5 s utan nya ljudblock
        g.setColour(kDanger);
        g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
        g.drawText("INGET LJUD FRÅN MOTORN – kontrollera ljudkortet",
                   getLocalBounds().removeFromTop(48), juce::Justification::centred);
    }
}

void GuitarFXEditor::resized() {
    auto area = getLocalBounds().reduced(16);
    area.removeFromBottom(36);
    area.removeFromRight(160);
    area.removeFromTop(48);

    inputChannel_.setBounds(area.removeFromTop(32).removeFromLeft(200));
    area.removeFromTop(12);

    const int knob = juce::jmin(area.getWidth() / 2, area.getHeight() - 24);
    auto left = area.removeFromLeft(area.getWidth() / 2);
    inputLabel_.setBounds(left.removeFromTop(24));
    inputGain_.setBounds(left.withSizeKeepingCentre(knob, knob));
    outputLabel_.setBounds(area.removeFromTop(24));
    outputGain_.setBounds(area.withSizeKeepingCentre(knob, knob));
}
