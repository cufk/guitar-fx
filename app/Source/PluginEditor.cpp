#include "PluginEditor.h"

// Definieras i StandaloneApp.cpp (gör inget i plugin-versionerna).
void guitarfxShowAudioSettings(juce::Component* parent);

namespace {
juce::String S(const char* utf8) { return juce::String::fromUTF8(utf8); }
}

GuitarFXEditor::GuitarFXEditor(GuitarFXProcessor& p)
    : AudioProcessorEditor(&p), processor_(p), stage_(p), grid_(p, [this](int i) { panel_.show(i); }), panel_(p), presets_(p) {
    applyAppearance(true);
    setLookAndFeel(&lnf_);
    // Menyer, dialoger och popovers ritas i egna fönster – ge dem samma tema.
    juce::LookAndFeel::setDefaultLookAndFeel(&lnf_);

    header_.input.onClick = [this] {
        auto panel = std::make_unique<ui::InputPanel>(processor_);
        // Inuti fönstret (parent = this) så att panelen ärver temat.
        juce::CallOutBox::launchAsynchronously(std::move(panel), getLocalArea(&header_, header_.input.getBounds()), this);
    };
    header_.more.onClick = [this] { showMoreMenu(); };

    for (juce::Component* c : std::initializer_list<juce::Component*> { &header_, &stage_, &grid_, &panel_, &presets_ })
        addAndMakeVisible(c);

    setWantsKeyboardFocus(true);
    setResizable(true, true);
    setResizeLimits(1040, 720, 2600, 1800);
    setSize(1320, 880);
    startTimerHz(30);
}

GuitarFXEditor::~GuitarFXEditor() {
    processor_.engine().latencyProbe().cancel();
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
}

void GuitarFXEditor::applyAppearance(bool force) {
    const int mode = processor_.appearance.load();
    const bool dark = mode == 2 || (mode == 0 && juce::Desktop::getInstance().isDarkModeActive());
    if (!force && dark == dark_) return;
    dark_ = dark;
    ui::setDarkMode(dark);
    lnf_.refreshColours();
    sendLookAndFeelChange();
    if (auto* top = getTopLevelComponent()) {
        if (auto* window = dynamic_cast<juce::DocumentWindow*>(top)) window->setBackgroundColour(ui::palette().background);
    }
    repaint();
}

juce::String GuitarFXEditor::latencyText(bool shortForm) const {
    using St = gfx::LatencyProbe::Status;
    const auto& probe = processor_.engine().latencyProbe();
    const auto ms = juce::String(probe.resultMs(), 2).replaceCharacter('.', ',') + " ms";
    switch (probe.status()) {
        case St::Running:      return S("Mäter…");
        case St::Done:         return shortForm ? ms : S("Rundtur ") + ms;
        case St::NoSignal:     return shortForm ? S("Ingen signal") : S("Latens: ingen signal tillbaka");
        case St::Inconsistent: return shortForm ? S("Ostabil") : S("Latens: ostabilt – mät igen");
        case St::Idle:         break;
    }
    return {};
}

void GuitarFXEditor::timerCallback() {
    auto& e = processor_.engine();
    stage_.update(e.inputPeak(0), e.inputPeak(1), e.outputPeak(), latencyText(true));
    grid_.refresh();
    panel_.refresh();
    presets_.refresh();

    auto& rig = processor_.rig();
    juce::String subtitle = rig.activePreset() < 0 ? S("Grundljud – inget preset aktivt")
                          : S("Preset ") + juce::String(rig.activePreset() + 1) + S(" – ") + ui::u8(rig.activeName());
    if (e.monoOutput.load()) subtitle << S(" · Mono ut");
    const int mode = e.input().mode.load();
    header_.setTexts(subtitle, mode == 2 ? S("Blanda 1 + 2") : mode == 1 ? S("Ingång 2") : S("Ingång 1"));

    // Watchdog: har ljudmotorn levererat nya block sedan förra kollen?
    const auto blocks = e.processedBlocks();
    const int before = stalledTicks_;
    stalledTicks_ = (blocks == lastBlockCount_) ? stalledTicks_ + 1 : 0;
    lastBlockCount_ = blocks;
    if ((before > 15) != (stalledTicks_ > 15)) repaint();

    if (++tick_ % 15 == 0) applyAppearance(false);
}

void GuitarFXEditor::paint(juce::Graphics& g) {
    g.fillAll(ui::palette().background);
}

// Ritas ovanpå allt så att varningen aldrig döljs.
void GuitarFXEditor::paintOverChildren(juce::Graphics& g) {
    if (stalledTicks_ <= 15) return; // ~0,5 s utan nya ljudblock
    const auto text = S("Inget ljud från motorn – kontrollera ljudkortet");
    const int w = static_cast<int>(juce::GlyphArrangement::getStringWidth(ui::font(15.0f, ui::Weight::SemiBold), text)) + 56;
    auto r = juce::Rectangle<int>(w, 44).withCentre({ getWidth() / 2, header_.getY() + 28 });
    ui::drawTile(g, r.toFloat(), 22.0f, ui::palette().danger);
    g.setColour(juce::Colours::white);
    g.fillEllipse(r.getX() + 18.0f, r.getCentreY() - 4.0f, 8.0f, 8.0f);
    g.setFont(ui::font(15.0f, ui::Weight::SemiBold));
    g.drawText(text, r.withTrimmedLeft(24), juce::Justification::centred);
}

void GuitarFXEditor::resized() {
    auto r = getLocalBounds().reduced(ui::size::page, 0);
    r.removeFromTop(28);
    header_.setBounds(r.removeFromTop(84));
    r.removeFromTop(20);
    r.removeFromBottom(28);
    presets_.setBounds(r.removeFromBottom(56));
    r.removeFromBottom(28);
    stage_.setBounds(r.removeFromLeft(juce::jlimit(340, 520, r.getWidth() * 34 / 100)));
    r.removeFromLeft(40);
    grid_.setBounds(r.removeFromTop(150));
    r.removeFromTop(20);
    panel_.setBounds(r);
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
    const bool standalone = juce::JUCEApplicationBase::isStandaloneApp();
    if (standalone) m.addItem(1, S("Ljudinställningar…"));
    m.addItem(2, S("Mät latens…"));
    m.addSeparator();
    m.addItem(3, S("Exportera presets…"));
    m.addItem(4, S("Importera presets…"));
    m.addSeparator();
    juce::PopupMenu look;
    const int a = processor_.appearance.load();
    look.addItem(20, S("Följ systemet"), true, a == 0);
    look.addItem(21, S("Ljust"), true, a == 1);
    look.addItem(22, S("Mörkt"), true, a == 2);
    m.addSubMenu(S("Utseende"), look);
    juce::PopupMenu help;
    help.addItem(30, S("Tangenter: 1–9, 0/Esc, ←/→"), false);
    help.addItem(31, S("MIDI Program Change 0–8 = preset 1–9"), false);
    help.addItem(32, S("MIDI CC 102/103 = föregående/nästa, 104 = grundljud"), false);
    m.addSubMenu(S("Fotswitch och tangenter"), help);
    juce::Component::SafePointer<GuitarFXEditor> safe(this);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&header_.more).withMinimumWidth(240), [safe](int r) {
        if (!safe) return;
        if (r == 1) guitarfxShowAudioSettings(safe.getComponent());
        if (r == 2) safe->startLatencyMeasurement();
        if (r == 3) safe->exportPresets();
        if (r == 4) safe->importPresets();
        if (r >= 20 && r <= 22) { safe->processor_.appearance.store(r - 20); safe->applyAppearance(true); }
    });
}

void GuitarFXEditor::startLatencyMeasurement() {
    auto& probe = processor_.engine().latencyProbe();
    if (probe.isActive()) { probe.cancel(); return; }
    juce::Component::SafePointer<GuitarFXEditor> safe(this);
    juce::AlertWindow::showAsync(
        juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::NoIcon)
            .withTitle(S("Mät latens"))
            .withMessage(S("Koppla en kabel från Line Out Left (baksidan) till ingång 1 (ingång 2 om källan är Ingång 2).\n\n"
                           "INST av, 48V av, gain lågt, Direct Monitor av. Koppla bort förstärkare och högtalare – "
                           "mätningen skickar ut klick på −12 dBFS.\n\nDet tar ca 4 sekunder. Resultatet visas längst ned."))
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
