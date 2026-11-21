#include "PluginEditor.h"

namespace {
juce::String S(const char* utf8) { return juce::String::fromUTF8(utf8); }
}

GuitarFXEditor::GuitarFXEditor(GuitarFXProcessor& p)
    : AudioProcessorEditor(&p), processor_(p), effects_(p), presets_(p), bottom_(p) {
    dark_ = juce::Desktop::getInstance().isDarkModeActive();
    ui::setDarkMode(dark_);
    lnf_.refreshColours();
    setLookAndFeel(&lnf_);

    header_.input.onClick = [this] {
        auto panel = std::make_unique<ui::InputPanel>(processor_);
        // Inuti fönstret (parent = this) så att panelen ärver temat.
        juce::CallOutBox::launchAsynchronously(std::move(panel), getLocalArea(&header_, header_.input.getBounds()), this);
    };
    header_.more.onClick = [this] { showMoreMenu(); };

    addAndMakeVisible(header_);
    addAndMakeVisible(effects_);
    addAndMakeVisible(presets_);
    addAndMakeVisible(bottom_);

    setWantsKeyboardFocus(true);
    setResizable(true, true);
    setResizeLimits(960, 640, 2600, 1800);
    setSize(1240, 820);
    startTimerHz(30);
}

GuitarFXEditor::~GuitarFXEditor() {
    processor_.engine().latencyProbe().cancel();
    setLookAndFeel(nullptr);
}

juce::String GuitarFXEditor::latencyText() const {
    using St = gfx::LatencyProbe::Status;
    const auto& probe = processor_.engine().latencyProbe();
    switch (probe.status()) {
        case St::Running:      return S("Mäter latens…");
        case St::Done:         return S("Rundtur ") + juce::String(probe.resultMs(), 2).replaceCharacter('.', ',') + " ms";
        case St::NoSignal:     return S("Latens: ingen signal tillbaka");
        case St::Inconsistent: return S("Latens: ostabilt – mät igen");
        case St::Idle:         break;
    }
    return {};
}

void GuitarFXEditor::timerCallback() {
    auto& e = processor_.engine();
    bottom_.update(e.inputPeak(0), e.inputPeak(1), e.outputPeak());
    effects_.refresh();
    presets_.refresh();

    auto& rig = processor_.rig();
    juce::String subtitle = rig.activePreset() < 0 ? S("Grundljud")
                          : S("Preset ") + juce::String(rig.activePreset() + 1) + S(" · ") + ui::u8(rig.activeName());
    const auto lat = latencyText();
    if (lat.isNotEmpty()) subtitle << S(" · ") << lat;
    header_.setTexts("GuitarFX", subtitle);

    // Watchdog: har ljudmotorn levererat nya block sedan förra kollen?
    const auto blocks = e.processedBlocks();
    const int before = stalledTicks_;
    stalledTicks_ = (blocks == lastBlockCount_) ? stalledTicks_ + 1 : 0;
    lastBlockCount_ = blocks;
    if ((before > 15) != (stalledTicks_ > 15)) repaint();

    // Ljust/mörkt läge följer systemet.
    if (++tick_ % 30 == 0) {
        const bool dark = juce::Desktop::getInstance().isDarkModeActive();
        if (dark != dark_) {
            dark_ = dark;
            ui::setDarkMode(dark);
            lnf_.refreshColours();
            sendLookAndFeelChange();
            repaint();
        }
    }
}

void GuitarFXEditor::paint(juce::Graphics& g) {
    g.fillAll(ui::palette().background);
}

// Ritas ovanpå allt så att varningen aldrig döljs.
void GuitarFXEditor::paintOverChildren(juce::Graphics& g) {
    if (stalledTicks_ > 15) { // ~0,5 s utan nya ljudblock
        auto r = getLocalBounds().removeFromTop(56);
        g.setColour(ui::palette().danger.withAlpha(0.95f));
        g.fillRect(r);
        g.setColour(juce::Colours::white);
        g.setFont(ui::font(20.0f, true));
        g.drawText(S("INGET LJUD FRÅN MOTORN – kontrollera ljudkortet"), r, juce::Justification::centred);
    }
}

void GuitarFXEditor::resized() {
    auto r = getLocalBounds().reduced(28, 0);
    r.removeFromTop(18);
    header_.setBounds(r.removeFromTop(52));
    r.removeFromTop(14);
    r.removeFromBottom(20);
    bottom_.setBounds(r.removeFromBottom(72));
    r.removeFromBottom(16);
    const int presetW = juce::jlimit(320, 520, r.getWidth() * 38 / 100);
    presets_.setBounds(r.removeFromRight(presetW));
    r.removeFromRight(20);
    effects_.setBounds(r);
}

// Ta tangentbordsfokus när fönstret visas, så att 1–9/←/→ (och pedaler som
// skickar tangenttryck) fungerar direkt utan att man först klickar i appen.
void GuitarFXEditor::visibilityChanged() {
    if (isShowing()) juce::MessageManager::callAsync([safe = juce::Component::SafePointer<GuitarFXEditor>(this)] {
        if (safe) safe->grabKeyboardFocus();
    });
}

bool GuitarFXEditor::keyPressed(const juce::KeyPress& k) {
    auto& rig = processor_.rig();
    const auto c = k.getTextCharacter();
    if (c >= '1' && c <= '9') { rig.select(static_cast<int>(c - '1')); return true; }
    if (c == '0' || k == juce::KeyPress::escapeKey) { rig.activate(-1); return true; }
    if (k == juce::KeyPress::rightKey || k == juce::KeyPress::pageDownKey) { rig.selectNext(); return true; }
    if (k == juce::KeyPress::leftKey || k == juce::KeyPress::pageUpKey) { rig.selectPrevious(); return true; }
    return false;
}

void GuitarFXEditor::showMoreMenu() {
    juce::PopupMenu m;
    m.addItem(1, S("Mät latens…"));
    m.addSeparator();
    m.addItem(2, S("Exportera presets…"));
    m.addItem(3, S("Importera presets…"));
    m.addSeparator();
    m.addSectionHeader(S("Fotswitch / MIDI"));
    m.addItem(10, S("Program Change 0–8 = preset 1–9"), false);
    m.addItem(11, S("CC 102/103 = föregående/nästa, CC 104 = grundljud"), false);
    m.addItem(12, S("Tangenter: 1–9, 0, ←/→ (pedaler som skickar tangenter)"), false);
    juce::Component::SafePointer<GuitarFXEditor> safe(this);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&header_.more), [safe](int r) {
        if (!safe) return;
        if (r == 1) safe->startLatencyMeasurement();
        if (r == 2) safe->exportPresets();
        if (r == 3) safe->importPresets();
    });
}

void GuitarFXEditor::startLatencyMeasurement() {
    auto& probe = processor_.engine().latencyProbe();
    if (probe.isActive()) { probe.cancel(); return; }
    juce::Component::SafePointer<GuitarFXEditor> safe(this);
    juce::AlertWindow::showAsync(
        juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::WarningIcon)
            .withTitle(S("Mät latens"))
            .withMessage(S("Koppla en kabel från Line Out Left (baksidan) till ingång 1 (ingång 2 om källan är Ingång 2).\n"
                           "INST av, 48V av, gain lågt, Direct Monitor av.\n"
                           "Koppla bort förstärkare/högtalare – mätningen skickar ut klick på -12 dBFS.\n"
                           "Mätningen tar ca 4 sekunder. Resultatet visas under titeln."))
            .withButton("Starta", 1)
            .withButton("Avbryt", 0)
            .withAssociatedComponent(this),
        [safe](int result) {
            if (safe != nullptr && result == 1) safe->processor_.engine().latencyProbe().start();
        });
}

void GuitarFXEditor::exportPresets() {
    chooser_ = std::make_unique<juce::FileChooser>(S("Exportera presets"),
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("GuitarFX-presets.json"), "*.json");
    chooser_->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this](const juce::FileChooser& fc) {
                              const auto f = fc.getResult();
                              if (f == juce::File()) return;
                              f.replaceWithText(juce::JSON::toString(processor_.rigToJson()));
                          });
}

void GuitarFXEditor::importPresets() {
    chooser_ = std::make_unique<juce::FileChooser>(S("Importera presets"),
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.json");
    chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this](const juce::FileChooser& fc) {
                              const auto f = fc.getResult();
                              if (f == juce::File()) return;
                              if (!processor_.rigFromJson(juce::JSON::parse(f)))
                                  juce::AlertWindow::showAsync(juce::MessageBoxOptions().withTitle(S("Kunde inte importera"))
                                      .withMessage(S("Filen är inte en GuitarFX-presetfil.")).withButton("OK", 0), nullptr);
                          });
}
