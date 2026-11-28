#include "PluginEditor.h"

// Definieras i StandaloneApp.cpp (gör inget i plugin-versionerna).
void guitarfxShowAudioSettings(juce::Component* parent);

namespace {
juce::String S(const char* utf8) { return juce::String::fromUTF8(utf8); }
}

GuitarFXEditor::GuitarFXEditor(GuitarFXProcessor& p)
    : AudioProcessorEditor(&p), processor_(p), setlist_(p), meter_(p), grid_(p, [this](int i) { panel_.show(i); }), panel_(p), tuner_(p), analysisView_(p), saver_(p) {
    applyAppearance(true);
    setLookAndFeel(&lnf_);
    // Menyer, dialoger och popovers ritas i egna fönster – ge dem samma tema.
    juce::LookAndFeel::setDefaultLookAndFeel(&lnf_);

    header_.more.onClick = [this] { showMoreMenu(); };
    header_.boost.onClick = [this] { processor_.toggleBoost(); };
    header_.screensaver.onClick = [this] { showScreensaverMenu(); };
    saver_.onWake = [this] { saver_.stop(); userActivity(); };
    juce::Desktop::getInstance().addGlobalMouseListener(&activity_);
    lastActivityMs_ = juce::Time::getMillisecondCounterHiRes();
    lastMidiEvents_ = p.midiEventCount();
    juce::Timer::callAfterDelay(1500, [safe = juce::Component::SafePointer<GuitarFXEditor>(this)] { if (safe) safe->checkStemSplitter(); });
    setlist_.analyse.onClick = [this] { setAnalysisOpen(!analysisView_.isOpen()); };
    analysisView_.onClose = [this] { setAnalysisOpen(false); };

    for (juce::Component* c : std::initializer_list<juce::Component*> { &header_, &meter_, &tuner_, &setlist_, &grid_, &panel_ })
        addAndMakeVisible(c);
    addChildComponent(analysisView_);
    addChildComponent(saver_);   // överst; varningar ritas ändå ovanpå (paintOverChildren)

    setWantsKeyboardFocus(true);
   #if JUCE_IOS
    // iPad: helskärm i liggande läge – fönstret bestämmer storleken (iPad mini: 1133 × 744).
    setResizable(false, false);
    setSize(1194, 834);
   #else
    setResizable(true, true);
    setResizeLimits(1040, 790, 2600, 1800);
    setSize(1320, 900);
   #endif
    startTimerHz(30);
}

GuitarFXEditor::~GuitarFXEditor() {
    juce::Desktop::getInstance().removeGlobalMouseListener(&activity_);
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
    meter_.update();
    grid_.refresh();
    // Boost: svart när den är på, visar hur mycket den höjer.
    auto& vol = processor_.volume();
    const bool boostOn = vol.boostOn();
    if (header_.boost.getToggleState() != boostOn) header_.boost.setToggleState(boostOn, juce::dontSendNotification);
    const float bdb = vol.boostDb.get();
    const auto boostText = "+" + juce::String(bdb, gfx::sameValue(bdb, std::floor(bdb)) ? 0 : 1).replaceCharacter('.', ',') + " dB";
    if (header_.boost.suffix != boostText) { header_.boost.suffix = boostText; header_.boost.repaint(); }
    if (meter_.pillWidth() != meterPillWidth_) resized();   // ingångsläget ändrat

    // Skärmsläckare: etikett, MIDI som aktivitet och start efter inaktivitet.
    const int mode = processor_.screensaver.load();
    const auto saverText = juce::String::fromUTF8(ui::ScreenSaver::shortName(mode));
    if (header_.screensaver.getButtonText() != saverText) header_.screensaver.setButtonText(saverText);
    if (const int midi = processor_.midiEventCount(); midi != lastMidiEvents_) { lastMidiEvents_ = midi; userActivity(); }
    const double idleMs = juce::Time::getMillisecondCounterHiRes() - lastActivityMs_;
    if (mode > 0 && !saver_.isRunning() && !analysisView_.isOpen()
        && juce::ModalComponentManager::getInstance()->getNumModalComponents() == 0
        && idleMs > processor_.screensaverMinutes.load() * 60000.0)
        { saverStartedMs_ = juce::Time::getMillisecondCounterHiRes(); saver_.start(mode); }
    panel_.refresh();
    setlist_.refresh();

    // Watchdog: har ljudmotorn levererat nya block sedan förra kollen?
    const auto blocks = e.processedBlocks();
    const int before = stalledTicks_;
    stalledTicks_ = (blocks == lastBlockCount_) ? stalledTicks_ + 1 : 0;
    lastBlockCount_ = blocks;
    if ((before > 15) != (stalledTicks_ > 15)) repaint();

    if (++tick_ % 15 == 0) applyAppearance(false);
}

void GuitarFXEditor::userActivity() {
    const double now = juce::Time::getMillisecondCounterHiRes();
    lastActivityMs_ = now;
    // Kort startperiod så att menyn som just stängdes inte väcker direkt.
    if (saver_.isRunning() && now - saverStartedMs_ > 600.0) saver_.stop();
}

void GuitarFXEditor::showScreensaverMenu() {
    const int cur = processor_.screensaver.load(), mins = processor_.screensaverMinutes.load();
    juce::PopupMenu m;
    m.addSectionHeader(S("Skärmsläckare"));
    for (int i = 0; i < ui::ScreenSaver::NumModes; ++i)
        m.addItem(1 + i, juce::String::fromUTF8(ui::ScreenSaver::modeName(i)), true, cur == i);
    m.addSeparator();
    juce::PopupMenu after;
    for (int v : { 1, 5, 15 }) after.addItem(100 + v, juce::String(v) + " min", true, mins == v);
    m.addSubMenu(S("Starta efter"), after, cur > 0);
    m.addItem(200, S("Visa nu"), cur > 0);
    juce::Component::SafePointer<GuitarFXEditor> safe(this);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&header_.screensaver), [safe](int r) {
        if (!safe || r == 0) return;
        auto& p = safe->processor_;
        if (r >= 1 && r <= ui::ScreenSaver::NumModes) p.screensaver.store(r - 1);
        else if (r > 100 && r < 200) p.screensaverMinutes.store(r - 100);
        else if (r == 200) juce::Timer::callAfterDelay(250, [safe] {   // efter att menyn stängts
            if (safe) { safe->saverStartedMs_ = juce::Time::getMillisecondCounterHiRes(); safe->saver_.start(safe->processor_.screensaver.load()); }
        });
        safe->userActivity();
    });
}

void GuitarFXEditor::checkStemSplitter() {
    auto all = stems::listProjects();
    if (all.empty()) return;
    auto& imported = processor_.stemsplitterImported;
    if (!processor_.stemsplitterKnown) {
        // Första gången: bara projekt från de senaste två veckorna räknas som nya.
        const auto cutoff = juce::Time::getCurrentTime() - juce::RelativeTime::days(14);
        for (const auto& p : all) if (p.created < cutoff) imported.addIfNotAlreadyThere(p.key);
        processor_.stemsplitterKnown = true;
    }
    std::vector<stems::Project> fresh;
    for (const auto& p : all) if (!imported.contains(p.key) && fresh.size() < 5) fresh.push_back(p);
    if (fresh.empty()) return;
    juce::String list;
    for (const auto& p : fresh) list << juce::String::fromUTF8("•  ") << p.name << "\n";
    const auto n = static_cast<int>(fresh.size());
    juce::Component::SafePointer<GuitarFXEditor> safe(this);
    juce::AlertWindow::showAsync(
        juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::NoIcon)
            .withTitle(n == 1 ? S("Ny låt i StemSplitter") : S("Nya låtar i StemSplitter"))
            .withMessage(list + S("\nVill du skapa ") + (n == 1 ? S("ett preset") : S("presets"))
                         + S(" där gitarren får plats i bandet? Stämmorna ger ett bättre underlag än hela låten."))
            .withButton(n == 1 ? S("Skapa preset") : S("Skapa presets"), 1)
            .withButton(S("Hoppa över"), 2)
            .withButton(S("Inte nu"), 0)
            .withAssociatedComponent(this),
        [safe, fresh](int r) {
            if (!safe) return;
            if (r == 1) { safe->setAnalysisOpen(true); safe->analysisView_.startBatch(fresh); }
            else if (r == 2) for (const auto& p : fresh) safe->processor_.stemsplitterImported.addIfNotAlreadyThere(p.key);
        });
}

void GuitarFXEditor::setAnalysisOpen(bool open) {
    analysisView_.setOpen(open);
    if (open) analysisView_.toFront(false);
    setlist_.analyse.setToggleState(open, juce::dontSendNotification);
}


void GuitarFXEditor::paint(juce::Graphics& g) {
    g.fillAll(ui::palette().background);
}

// Ritas ovanpå allt så att varningen aldrig döljs.
void GuitarFXEditor::paintOverChildren(juce::Graphics& g) {
    if (stalledTicks_ <= 15) return; // ~0,5 s utan nya ljudblock
    const auto text = S("Inget ljud från motorn – kontrollera ljudkortet");
    const int w = static_cast<int>(juce::GlyphArrangement::getStringWidth(ui::font(15.0f, ui::Weight::SemiBold), text)) + 56;
    auto r = juce::Rectangle<int>(w, 44).withCentre({ getWidth() / 2, header_.getBounds().getCentreY() });
    ui::drawTile(g, r.toFloat(), 22.0f, ui::palette().danger);
    g.setColour(juce::Colours::white);
    g.fillEllipse(static_cast<float>(r.getX()) + 18.0f, static_cast<float>(r.getCentreY()) - 4.0f, 8.0f, 8.0f);
    g.setFont(ui::font(15.0f, ui::Weight::SemiBold));
    g.drawText(text, r.withTrimmedLeft(24), juce::Justification::centred);
}

void GuitarFXEditor::resized() {
    // Ett rutnät: 32 px marginal runt om, sidhuvud 56, 24 px luft, innehåll, 24 px luft, stämfält 72.
    auto r = getLocalBounds().reduced(ui::size::page);
    auto head = r.removeFromTop(56);
    header_.setBounds(head);
    meter_.setBounds(head.withTrimmedRight(ui::size::button + 16).removeFromRight(440));   // vänster om ⋯
    // Boost direkt till vänster om nivåmätaren.
    meterPillWidth_ = meter_.pillWidth();
    const int boostW = 150, b = ui::size::button;
    const int boostRight = meter_.getRight() - meterPillWidth_ - 16 - head.getX();
    header_.boost.setBounds(boostRight - boostW, (head.getHeight() - b) / 2, boostW, b);
    // Skärmsläckare (rullgardin) till vänster om Boost – bredd för det längsta korta namnet.
    int saverW = 0;
    for (int i = 0; i < ui::ScreenSaver::NumModes; ++i) {
        header_.screensaver.setSize(10, b);
        saverW = juce::jmax(saverW, header_.screensaver.idealWidth(juce::String::fromUTF8(ui::ScreenSaver::shortName(i))));
    }
    header_.screensaver.setBounds(header_.boost.getX() - 16 - saverW, (head.getHeight() - b) / 2, saverW, b);
    r.removeFromTop(24);
    tuner_.setBounds(r.removeFromBottom(72));
    r.removeFromBottom(24);
    setlist_.setBounds(r.removeFromLeft(juce::jlimit(300, ui::size::sidebar, getWidth() * 28 / 100)));
    r.removeFromLeft(ui::size::columnGap);
    grid_.setBounds(r.removeFromTop(ui::EffectGrid::preferredHeight()));
    panel_.setBounds(r);
    analysisView_.setBounds(setlist_.getBounds().getUnion(panel_.getBounds()).getUnion(grid_.getBounds()));
    saver_.setBounds(getLocalBounds());
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
    if (saver_.isRunning()) { userActivity(); return true; }   // första tangenten väcker bara
    userActivity();
    if (c == 't' || c == 'T') { tuner_.toggleMute(); return true; }
    if (c == 'b' || c == 'B') { processor_.toggleBoost(); return true; }
    if (k == juce::KeyPress::spaceKey) { processor_.tapTempo(); return true; }
    if (analysisView_.isOpen() && k == juce::KeyPress::escapeKey) { setAnalysisOpen(false); return true; }
    if (c >= '1' && c <= '9') { rig.select(static_cast<int>(c - '1')); return true; }
    if (c == '0' || k == juce::KeyPress::escapeKey) { rig.activate(-1); return true; }
    if (k == juce::KeyPress::rightKey || k == juce::KeyPress::pageDownKey) { rig.selectNext(); return true; }
    if (k == juce::KeyPress::leftKey || k == juce::KeyPress::pageUpKey) { rig.selectPrevious(); return true; }
    return false;
}

void GuitarFXEditor::showInputPanel() {
    auto panel = std::make_unique<ui::InputPanel>(processor_);
    // Inuti fönstret (parent = this) så att panelen ärver temat.
    juce::CallOutBox::launchAsynchronously(std::move(panel), getLocalArea(&header_, header_.more.getBounds()), this);
}

void GuitarFXEditor::showMoreMenu() {
    juce::PopupMenu m;
    const bool standalone = juce::JUCEApplicationBase::isStandaloneApp();
    const int mode = processor_.engine().input().mode.load();
    m.addItem(6, S("Ingång och utgång…   ") + (mode == 2 ? S("Blanda 1 + 2") : mode == 1 ? S("Ingång 2") : S("Ingång 1")));
    if (standalone) m.addItem(1, S("Ljudinställningar…"));
    m.addItem(2, S("Mät latens…"));
    m.addItem(5, S("Analysera låt…"));
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
    {
        const double cpu = processor_.cpuLoad() * 100.0;
        const float gr = processor_.engine().limiterGainReductionDb();
        juce::String status = S("CPU ") + juce::String(juce::roundToInt(cpu)) + S(" %  ·  Limiter ")
                            + (gr < -0.05f ? juce::String(gr, 1).replaceCharacter('.', ',') + " dB" : S("vilar"));
        const auto lat = latencyText(true);
        if (lat.isNotEmpty()) status << S("  ·  Latens ") << lat;
        const auto midi = processor_.lastMidiDescription();
        if (midi.isNotEmpty()) status << S("  ·  MIDI ") << midi;
        m.addSeparator();
        m.addItem(40, status, false);
    }
    juce::PopupMenu help;
    help.addItem(30, S("Tangenter: 1–9, 0/Esc, ←/→"), false);
    help.addItem(31, S("MIDI Program Change 0–8 = preset 1–9"), false);
    help.addItem(32, S("MIDI CC 102/103 = föregående/nästa, 104 = grundljud"), false);
    help.addItem(33, S("T eller MIDI CC 105 = Mute (stämning)"), false);
    help.addItem(34, S("B eller MIDI CC 106 = Boost på/av"), false);
    help.addItem(35, S("Mellanslag eller MIDI CC 107 = Tap tempo (delay)"), false);
    m.addSubMenu(S("Fotswitch och tangenter"), help);
    juce::Component::SafePointer<GuitarFXEditor> safe(this);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&header_.more).withMinimumWidth(240), [safe](int r) {
        if (!safe) return;
        if (r == 1) guitarfxShowAudioSettings(safe.getComponent());
        if (r == 2) safe->startLatencyMeasurement();
        if (r == 3) safe->exportPresets();
        if (r == 4) safe->importPresets();
        if (r == 5) safe->setAnalysisOpen(true);
        if (r == 6) safe->showInputPanel();
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
   #if JUCE_IOS
    // iPad: filen skrivs först i appens mapp; Filer-appen kopierar den sedan dit man väljer.
    const auto file = ui::appDataDirectory().getChildFile("Stagebox-presets.json");
    file.getParentDirectory().createDirectory();
    file.replaceWithText(juce::JSON::toString(processor_.rigToJson()));
    chooser_ = std::make_unique<juce::FileChooser>(S("Exportera presets"), file, "*.json");
    chooser_->launchAsync(juce::FileBrowserComponent::saveMode, [](const juce::FileChooser&) {});
   #else
    chooser_ = std::make_unique<juce::FileChooser>(S("Exportera presets"),
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Stagebox-presets.json"), "*.json");
    chooser_->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this](const juce::FileChooser& fc) {
                              const auto f = fc.getResult();
                              if (f == juce::File()) return;
                              f.replaceWithText(juce::JSON::toString(processor_.rigToJson()));
                          });
   #endif
}

void GuitarFXEditor::importPresets() {
    chooser_ = std::make_unique<juce::FileChooser>(S("Importera presets"),
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.json");
    chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this](const juce::FileChooser& fc) {
                              const auto f = ui::localCopyOf(fc, juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("Stagebox-import"));
                              if (f == juce::File()) return;
                              if (!processor_.rigFromJson(juce::JSON::parse(f)))
                                  juce::AlertWindow::showAsync(juce::MessageBoxOptions().withTitle(S("Kunde inte importera"))
                                      .withMessage(S("Filen är inte en Stagebox-presetfil.")).withButton("OK", 0), nullptr);
                          });
}
