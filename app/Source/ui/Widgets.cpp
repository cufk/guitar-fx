#include "Widgets.h"

namespace ui {

namespace {
juce::String S(const char* utf8) { return juce::String::fromUTF8(utf8); }

void drawCaption(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> r, juce::Colour c,
                 juce::Justification j = juce::Justification::centredLeft) {
    g.setColour(c);
    g.setFont(captionFont());
    // Egen versalisering av å/ä/ö: toUpperCase() beror på systemets locale.
    auto up = text.toUpperCase().replace(S("å"), S("Å")).replace(S("ä"), S("Ä")).replace(S("ö"), S("Ö"));
    g.drawText(up, r, j, true);
}

juce::Colour levelColour(float peak) {
    const float db = juce::Decibels::gainToDecibels(peak, -100.0f);
    return db > -1.0f ? palette().danger : db > -12.0f ? palette().warn : palette().good;
}
} // namespace

// ---------------------------------------------------------------------------
// IconButton
// ---------------------------------------------------------------------------
IconButton::IconButton(Icon icon, const juce::String& tooltip) : juce::Button(tooltip), icon_(icon) {
    setTooltip(tooltip);
    setWantsKeyboardFocus(false);
}

void IconButton::paintButton(juce::Graphics& g, bool over, bool down) {
    const auto& p = palette();
    auto r = getLocalBounds().toFloat();
    const float d = juce::jmin(r.getWidth(), r.getHeight()) - 6.0f;
    auto circle = r.withSizeKeepingCentre(d, d);
    const bool active = getToggleState();
    if (filled || active) {
        juce::Path c;
        c.addEllipse(circle);
        juce::DropShadow(juce::Colours::black.withAlpha(isDarkMode() ? 0.3f : 0.08f), 8, { 0, 2 }).drawForPath(g, c);
        g.setColour(active ? p.accent : (down ? p.subtle : p.card));
        g.fillPath(c);
    } else if (over) {
        g.setColour(p.subtle);
        g.fillEllipse(circle);
    }

    const auto ink = active ? juce::Colours::white : (isEnabled() ? p.ink : p.secondaryInk);
    const float s = d * 0.22f;
    const auto c = circle.getCentre();
    juce::Path path;
    const juce::PathStrokeType stroke(juce::jmax(1.6f, d * 0.055f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour(ink);
    switch (icon_) {
        case Icon::More:
            for (int i = -1; i <= 1; ++i) g.fillEllipse(c.x + i * s * 0.75f - 2.2f, c.y - 2.2f, 4.4f, 4.4f);
            return;
        case Icon::Plus:
            path.startNewSubPath(c.x - s, c.y); path.lineTo(c.x + s, c.y);
            path.startNewSubPath(c.x, c.y - s); path.lineTo(c.x, c.y + s);
            break;
        case Icon::Prev:
            path.startNewSubPath(c.x + s * 0.35f, c.y - s * 0.75f); path.lineTo(c.x - s * 0.4f, c.y); path.lineTo(c.x + s * 0.35f, c.y + s * 0.75f);
            break;
        case Icon::Next:
            path.startNewSubPath(c.x - s * 0.35f, c.y - s * 0.75f); path.lineTo(c.x + s * 0.4f, c.y); path.lineTo(c.x - s * 0.35f, c.y + s * 0.75f);
            break;
        case Icon::ChevronDown:
            path.startNewSubPath(c.x - s * 0.7f, c.y - s * 0.3f); path.lineTo(c.x, c.y + s * 0.4f); path.lineTo(c.x + s * 0.7f, c.y - s * 0.3f);
            break;
        case Icon::ChevronUp:
            path.startNewSubPath(c.x - s * 0.7f, c.y + s * 0.3f); path.lineTo(c.x, c.y - s * 0.4f); path.lineTo(c.x + s * 0.7f, c.y + s * 0.3f);
            break;
        case Icon::Sliders: {
            // Tre reglage (som SF Symbols "slider.horizontal.3").
            const float knobs[3] = { 0.35f, -0.4f, 0.1f };
            for (int i = 0; i < 3; ++i) {
                const float y = c.y + (i - 1) * s * 0.75f;
                path.startNewSubPath(c.x - s, y); path.lineTo(c.x + s, y);
                g.fillEllipse(c.x + knobs[i] * s - 3.2f, y - 3.2f, 6.4f, 6.4f);
            }
            break;
        }
    }
    g.strokePath(path, stroke);
}

// ---------------------------------------------------------------------------
// Chip
// ---------------------------------------------------------------------------
Chip::Chip(const juce::String& label, juce::Colour onColour) : juce::Button(label), onColour_(onColour) {
    setClickingTogglesState(true);
    setWantsKeyboardFocus(false);
}

void Chip::paintButton(juce::Graphics& g, bool over, bool) {
    const auto& p = palette();
    const bool on = getToggleState();
    auto r = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(on ? onColour_ : (over ? p.fill : p.subtle));
    g.fillRoundedRectangle(r, juce::jmin(7.0f, r.getHeight() * 0.35f));
    g.setColour(on ? juce::Colours::white : (isEnabled() ? p.secondaryInk : p.secondaryInk.withAlpha(0.5f)));
    g.setFont(font(12.0f, true));
    g.drawText(getButtonText(), getLocalBounds(), juce::Justification::centred, false);
}

// ---------------------------------------------------------------------------
// ParamControl
// ---------------------------------------------------------------------------
ParamControl::ParamControl(const gfx::ParamSpec& spec, juce::Colour tint) : spec_(spec) {
    if (!spec_.choices.empty()) {
        for (size_t i = 0; i < spec_.choices.size(); ++i) {
            auto c = std::make_unique<Chip>(u8(spec_.choices[i]), tint);
            c->setRadioGroupId(1);
            c->onClick = [this, i] { spec_.param->set(static_cast<float>(i)); };
            addAndMakeVisible(*c);
            choices_.push_back(std::move(c));
        }
    } else {
        slider_.setSliderStyle(juce::Slider::LinearHorizontal);
        slider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider_.setRange(spec_.param->min(), spec_.param->max(), spec_.step);
        if (spec_.logScale && spec_.param->min() > 0.0f)
            slider_.setSkewFactorFromMidPoint(std::sqrt(static_cast<double>(spec_.param->min()) * spec_.param->max()));
        slider_.setColour(juce::Slider::thumbColourId, tint);
        slider_.setWantsKeyboardFocus(false);
        slider_.onValueChange = [this] { spec_.param->set(static_cast<float>(slider_.getValue())); repaint(); };
        addAndMakeVisible(slider_);
    }
    refresh();
}

void ParamControl::refresh() {
    const float v = spec_.param->get();
    if (!choices_.empty()) {
        const int idx = juce::roundToInt(v);
        for (size_t i = 0; i < choices_.size(); ++i)
            choices_[i]->setToggleState(static_cast<int>(i) == idx, juce::dontSendNotification);
    } else if (!slider_.isMouseButtonDown() && std::abs(slider_.getValue() - v) > 1e-6) {
        slider_.setValue(v, juce::dontSendNotification);
        repaint();
    }
}

void ParamControl::paint(juce::Graphics& g) {
    auto top = getLocalBounds().removeFromTop(18).reduced(4, 0);
    drawCaption(g, u8(spec_.label), top, palette().secondaryInk);
    if (choices_.empty()) {
        g.setColour(palette().ink);
        g.setFont(font(13.0f, true));
        g.drawText(formatValue(spec_, spec_.param->get()), top, juce::Justification::centredRight, false);
    }
}

void ParamControl::resized() {
    auto r = getLocalBounds();
    r.removeFromTop(20);
    if (!choices_.empty()) {
        auto row = r.removeFromTop(26);
        const int w = row.getWidth() / static_cast<int>(choices_.size());
        for (auto& c : choices_) c->setBounds(row.removeFromLeft(w).reduced(2, 0));
    } else {
        slider_.setBounds(r.removeFromTop(24));
    }
}

// ---------------------------------------------------------------------------
// EffectRow
// ---------------------------------------------------------------------------
EffectRow::EffectRow(GuitarFXProcessor& proc, const gfx::EffectSpec& spec, std::function<void()> onLayoutChange)
    : proc_(proc), spec_(spec), onLayoutChange_(std::move(onLayoutChange)),
      onChip_(S("PÅ"), juce::Colour(spec.colour)), presetChip_("I PRESET", palette().accent) {
    onChip_.setTooltip(S("Slå på/av effekten"));
    onChip_.onClick = [this] { proc_.engine().chain().setEnabled(spec_.slot, onChip_.getToggleState()); };
    presetChip_.setTooltip(S("Ta med effekten i det aktiva presetet. Av = effekten följer grundljudet."));
    presetChip_.onClick = [this] { proc_.rig().setIncludedInActive(spec_.id, presetChip_.getToggleState()); };
    expand_.filled = false;
    expand_.onClick = [this] { setExpanded(!expanded_); };
    addAndMakeVisible(onChip_);
    addChildComponent(presetChip_);
    addAndMakeVisible(expand_);

    for (const auto& p : spec_.params) {
        params_.push_back(std::make_unique<ParamControl>(p, juce::Colour(spec_.colour)));
        addChildComponent(*params_.back());
    }
    if (spec_.id == "cabinet") {
        loadIr_ = std::make_unique<juce::TextButton>("Ladda IR...");
        builtInIr_ = std::make_unique<juce::TextButton>("Inbyggd");
        loadIr_->onClick = [this] {
            chooser_ = std::make_unique<juce::FileChooser>(S("Välj kabinett-IR"), proc_.impulseResponseFile(), "*.wav;*.aif;*.aiff");
            chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this](const juce::FileChooser& fc) {
                                      if (fc.getResult() != juce::File()) irStatus_ = proc_.loadImpulseResponse(fc.getResult());
                                  });
        };
        builtInIr_->onClick = [this] { proc_.useBuiltInImpulseResponse(); irStatus_.clear(); };
        addChildComponent(*loadIr_);
        addChildComponent(*builtInIr_);
    }
    refresh();
}

int EffectRow::preferredHeight(int width) const {
    if (!expanded_) return kHeader;
    const int cols = columns(width);
    const int rows = (static_cast<int>(params_.size()) + cols - 1) / cols;
    return kHeader + rows * (ParamControl::kHeight + 6) + (loadIr_ ? 44 : 0) + 10;
}

void EffectRow::setExpanded(bool e) {
    expanded_ = e;
    expand_.setIcon(e ? IconButton::Icon::ChevronUp : IconButton::Icon::ChevronDown);
    for (auto& p : params_) p->setVisible(e);
    if (loadIr_) { loadIr_->setVisible(e); builtInIr_->setVisible(e); }
    if (onLayoutChange_) onLayoutChange_();
}

void EffectRow::refresh() {
    auto& rig = proc_.rig();
    const bool on = proc_.engine().chain().isEnabled(spec_.slot);
    onChip_.setToggleState(on, juce::dontSendNotification);
    const bool presetActive = rig.activePreset() >= 0;
    presetChip_.setVisible(presetActive);
    presetChip_.setToggleState(presetActive && rig.activeIncludes(spec_.id), juce::dontSendNotification);
    if (expanded_) for (auto& p : params_) p->refresh();
    repaint();
}

void EffectRow::paint(juce::Graphics& g) {
    const auto& p = palette();
    auto r = getLocalBounds().toFloat().reduced(2.0f, 3.0f);
    drawCard(g, r, 12.0f);
    const auto colour = juce::Colour(spec_.colour);
    const bool on = onChip_.getToggleState();
    auto& rig = proc_.rig();
    const bool inPreset = rig.activeIncludes(spec_.id);

    // Färgad kant till vänster när effekten hörs.
    if (on) {
        g.saveState();
        juce::Path clip;
        clip.addRoundedRectangle(r, 12.0f);
        g.reduceClipRegion(clip);
        g.setColour(colour);
        g.fillRect(r.withWidth(5.0f));
        g.restoreState();
    }

    auto header = getLocalBounds().removeFromTop(kHeader).reduced(18, 0);
    auto dot = header.removeFromLeft(14).toFloat().withSizeKeepingCentre(12.0f, 12.0f);
    if (on) { g.setColour(colour); g.fillEllipse(dot); }
    else { g.setColour(colour.withAlpha(0.8f)); g.drawEllipse(dot.reduced(1.0f), 2.0f); }
    header.removeFromLeft(12);

    auto text = header.removeFromLeft(juce::jmax(120, header.getWidth() - 230));
    g.setColour(p.ink);
    g.setFont(font(16.0f, true));
    g.drawText(u8(spec_.name), text.removeFromTop(text.getHeight() / 2 + 4).withTrimmedTop(8), juce::Justification::bottomLeft, true);

    juce::String status;
    juce::Colour statusColour = p.secondaryInk;
    if (rig.activePreset() < 0) status = on ? S("På i grundljudet") : S("Av");
    else if (inPreset) { status = S("I presetet ") + u8(rig.activeName()) + (on ? "" : S(" (av)")); statusColour = p.accent; }
    else status = on ? S("Grundljud") : S("Av · grundljud");
    if (spec_.id == "cabinet" && irStatus_.isEmpty()) status << S(" · ") << u8(proc_.cabinet().impulseResponseName());
    if (irStatus_.isNotEmpty()) { status = irStatus_; statusColour = p.danger; }
    g.setColour(statusColour);
    g.setFont(font(12.5f));
    g.drawText(status, text.withTrimmedBottom(6), juce::Justification::topLeft, true);
}

void EffectRow::resized() {
    auto header = getLocalBounds().removeFromTop(kHeader).reduced(16, 0);
    expand_.setBounds(header.removeFromRight(36).withSizeKeepingCentre(36, 36));
    header.removeFromRight(8);
    onChip_.setBounds(header.removeFromRight(46).withSizeKeepingCentre(46, 26));
    header.removeFromRight(8);
    presetChip_.setBounds(header.removeFromRight(84).withSizeKeepingCentre(84, 26));

    auto body = getLocalBounds().withTrimmedTop(kHeader).reduced(18, 0);
    const int cols = columns(getWidth());
    const int colW = body.getWidth() / cols;
    for (size_t i = 0; i < params_.size(); ++i) {
        const int row = static_cast<int>(i) / cols, col = static_cast<int>(i) % cols;
        params_[i]->setBounds(body.getX() + col * colW + 4, body.getY() + row * (ParamControl::kHeight + 6),
                              colW - 16, ParamControl::kHeight);
    }
    if (loadIr_) {
        const int rows = (static_cast<int>(params_.size()) + cols - 1) / cols;
        auto r = juce::Rectangle<int>(body.getX() + 4, body.getY() + rows * (ParamControl::kHeight + 6), 260, 34);
        loadIr_->setBounds(r.removeFromLeft(130));
        r.removeFromLeft(8);
        builtInIr_->setBounds(r);
    }
}

void EffectRow::mouseUp(const juce::MouseEvent& e) {
    if (e.y < kHeader && e.mouseWasClicked()) setExpanded(!expanded_);
}

// ---------------------------------------------------------------------------
// EffectList
// ---------------------------------------------------------------------------
EffectList::EffectList(GuitarFXProcessor& proc) {
    viewport_.setViewedComponent(&content_, false);
    viewport_.setScrollBarsShown(true, false);
    viewport_.setScrollBarThickness(6);
    addAndMakeVisible(viewport_);
    for (const auto& spec : proc.rig().effects()) {
        rows_.push_back(std::make_unique<EffectRow>(proc, spec, [this] { layoutRows(); }));
        content_.addAndMakeVisible(*rows_.back());
    }
    if (!rows_.empty()) rows_.front()->setExpanded(true);  // EQ utfälld från start
}

void EffectList::resized() {
    viewport_.setBounds(getLocalBounds());
    layoutRows();
}

void EffectList::layoutRows() {
    const int w = viewport_.getWidth() - 10;  // plats för rullningslisten
    int y = 0;
    for (auto& r : rows_) {
        const int h = r->preferredHeight(w);
        r->setBounds(0, y, w, h);
        y += h + 6;
    }
    content_.setSize(w, y);
}

// ---------------------------------------------------------------------------
// PresetPanel
// ---------------------------------------------------------------------------
PresetPanel::PresetPanel(GuitarFXProcessor& proc) : proc_(proc) {
    add_.onClick = [this] {
        auto& rig = proc_.rig();
        const int i = rig.addPreset("Preset " + std::to_string(rig.presets().size() + 1));
        rig.activate(i);
        rename(i);
    };
    addAndMakeVisible(add_);
}

void PresetPanel::refresh() {
    if (proc_.rig().revision() != lastRevision_) { lastRevision_ = proc_.rig().revision(); repaint(); }
}

juce::Rectangle<int> PresetPanel::tileBounds(int tile) const {
    const int cols = grid_.getWidth() >= 420 ? 3 : 2;
    const int n = static_cast<int>(proc_.rig().presets().size()) + 1;
    const int rows = (n + cols - 1) / cols;
    const int gap = 10;
    const int tileW = (grid_.getWidth() - gap * (cols - 1)) / cols;
    const int tileH = juce::jlimit(56, 96, (grid_.getHeight() - gap * (rows - 1)) / juce::jmax(1, rows));
    const int row = tile / cols, col = tile % cols;
    return { grid_.getX() + col * (tileW + gap), grid_.getY() + row * (tileH + gap), tileW, tileH };
}

int PresetPanel::tileAt(juce::Point<int> pt) const {
    const int n = static_cast<int>(proc_.rig().presets().size()) + 1;
    for (int t = 0; t < n; ++t) if (tileBounds(t).contains(pt)) return t;
    return -1;
}

void PresetPanel::paint(juce::Graphics& g) {
    const auto& p = palette();
    drawCard(g, getLocalBounds().toFloat().reduced(2.0f));
    auto head = getLocalBounds().reduced(20, 0).removeFromTop(64);
    g.setColour(p.ink);
    g.setFont(font(17.0f, true));
    g.drawText("Presets", head.removeFromTop(40).withTrimmedTop(14), juce::Justification::bottomLeft);
    g.setColour(p.secondaryInk);
    g.setFont(font(12.5f));
    g.drawText(S("Tryck för att aktivera, igen för grundljud · 1–9, ←/→ eller pedal"), head, juce::Justification::topLeft, true);

    auto& rig = proc_.rig();
    const int n = static_cast<int>(rig.presets().size()) + 1;
    for (int t = 0; t < n; ++t) {
        const auto r = tileBounds(t).toFloat();
        const int presetIndex = t - 1;
        const bool active = rig.activePreset() == presetIndex;
        g.setColour(active ? p.accent : p.subtle);
        g.fillRoundedRectangle(r, 12.0f);

        auto inner = r.reduced(14.0f, 10.0f).toNearestInt();
        const auto textColour = active ? juce::Colours::white : p.ink;
        drawCaption(g, t == 0 ? S("0 · utan preset") : juce::String(t), inner.removeFromTop(16),
                    active ? juce::Colours::white.withAlpha(0.85f) : p.secondaryInk);
        g.setColour(textColour);
        g.setFont(font(16.0f, true));
        const auto name = t == 0 ? S("Grundljud") : u8(rig.presets()[size_t(presetIndex)].name);
        g.drawFittedText(name, inner.removeFromTop(juce::jmax(20, inner.getHeight() - 14)), juce::Justification::centredLeft, 2);

        // Prickar: vilka effekter som ingår (grundljud: vilka som är på där).
        float x = static_cast<float>(inner.getX());
        const float y = static_cast<float>(inner.getBottom()) - 9.0f;
        for (const auto& e : rig.effects()) {
            bool show = false;
            if (t == 0) { auto it = rig.base().find(e.id); show = it != rig.base().end() && it->second.enabled; }
            else { const auto& o = rig.presets()[size_t(presetIndex)].overrides; auto it = o.find(e.id); show = it != o.end(); }
            if (!show) continue;
            g.setColour(active ? juce::Colours::white : juce::Colour(e.colour));
            g.fillEllipse(x, y, 8.0f, 8.0f);
            x += 12.0f;
        }
    }
}

void PresetPanel::resized() {
    auto r = getLocalBounds().reduced(20, 0);
    add_.setBounds(r.removeFromTop(64).removeFromRight(44).withSizeKeepingCentre(44, 44));
    grid_ = r.withTrimmedBottom(20).withTrimmedTop(6);
}

void PresetPanel::mouseUp(const juce::MouseEvent& e) {
    if (!e.mouseWasClicked() && e.getLengthOfMousePress() < 500) return;
    const int t = tileAt(e.getPosition());
    if (t < 0) return;
    const int presetIndex = t - 1;
    if (presetIndex >= 0 && (e.mods.isPopupMenu() || e.getLengthOfMousePress() >= 500)) { showMenu(presetIndex); return; }
    if (presetIndex < 0) proc_.rig().activate(-1);
    else proc_.rig().select(presetIndex);
}

void PresetPanel::showMenu(int i) {
    auto& rig = proc_.rig();
    const int n = static_cast<int>(rig.presets().size());
    juce::PopupMenu m;
    m.addSectionHeader(u8(rig.presets()[size_t(i)].name));
    m.addItem(1, S("Byt namn…"));
    m.addItem(2, "Duplicera");
    m.addItem(3, S("Flytta tidigare"), i > 0);
    m.addItem(4, "Flytta senare", i < n - 1);
    m.addSeparator();
    m.addItem(5, S("Ta bort…"));
    juce::Component::SafePointer<PresetPanel> safe(this);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition(), [safe, i](int r) {
        if (!safe) return;
        auto& model = safe->proc_.rig();
        switch (r) {
            case 1: safe->rename(i); break;
            case 2: model.duplicatePreset(i); break;
            case 3: model.movePreset(i, i - 1); break;
            case 4: model.movePreset(i, i + 1); break;
            case 5:
                juce::AlertWindow::showAsync(
                    juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::QuestionIcon)
                        .withTitle(S("Ta bort ") + u8(model.presets()[size_t(i)].name) + "?")
                        .withMessage(S("Presetet tas bort. Grundljudet och andra presets påverkas inte."))
                        .withButton("Ta bort", 1).withButton("Avbryt", 0).withAssociatedComponent(safe.getComponent()),
                    [safe, i](int res) { if (safe && res == 1) safe->proc_.rig().removePreset(i); });
                break;
            default: break;
        }
    });
}

void PresetPanel::rename(int i) {
    auto& rig = proc_.rig();
    if (i < 0 || i >= static_cast<int>(rig.presets().size())) return;
    auto* w = new juce::AlertWindow(S("Namn på preset"), S("Ge presetet ett namn du känner igen på scen."),
                                    juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor("name", u8(rig.presets()[size_t(i)].name));
    w->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Avbryt", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<PresetPanel> safe(this);
    w->enterModalState(true, juce::ModalCallbackFunction::create([safe, w, i](int r) {
        if (!safe || r != 1) return;
        const auto name = w->getTextEditorContents("name").trim();
        if (name.isNotEmpty()) safe->proc_.rig().renamePreset(i, name.toStdString());
    }), true);
}

// ---------------------------------------------------------------------------
// InputPanel
// ---------------------------------------------------------------------------
InputPanel::InputPanel(GuitarFXProcessor& proc)
    : proc_(proc),
      balanceSpec_ { "balance", "Balans 1 – 2", "", &proc.engine().input().balance, 0.01f, 2, false, {} },
      alignSpec_   { "align", "Tidsjustering", "ms", &proc.engine().input().alignMs, 0.01f, 2, false, {} },
      inSpec_      { "in", "In-nivå", "dB", &proc.engine().inputGain().gainDb, 0.1f, 1, false, {} },
      outSpec_     { "out", "Ut-nivå", "dB", &proc.engine().outputGain().gainDb, 0.1f, 1, false, {} },
      src1_(S("Ingång 1"), palette().accent), src2_(S("Ingång 2"), palette().accent), srcBlend_("Blanda", palette().accent),
      inv1_(S("Ø 1"), palette().warn), inv2_(S("Ø 2"), palette().warn),
      balance_(balanceSpec_, palette().accent), align_(alignSpec_, palette().accent),
      inGain_(inSpec_, palette().secondaryInk), outGain_(outSpec_, palette().secondaryInk) {
    auto& in = proc_.engine().input();
    int mode = 0;
    for (auto* c : { &src1_, &src2_, &srcBlend_ }) {
        c->setRadioGroupId(7);
        c->onClick = [&in, mode] { in.mode.store(mode); };
        addAndMakeVisible(*c);
        ++mode;
    }
    inv1_.onClick = [this] { proc_.engine().input().invert1.store(inv1_.getToggleState()); };
    inv2_.onClick = [this] { proc_.engine().input().invert2.store(inv2_.getToggleState()); };
    inv1_.setTooltip(S("Vänd polariteten på ingång 1"));
    inv2_.setTooltip(S("Vänd polariteten på ingång 2"));
    autoAlign_.onClick = [this] {
        proc_.engine().input().requestCapture();
        alignText_ = S("Spela nu – lyssnar på båda ingångarna…");
        alignColour_ = palette().warn;
    };
    for (juce::Component* c : std::initializer_list<juce::Component*> { &inv1_, &inv2_, &balance_, &align_, &inGain_, &outGain_, &autoAlign_ })
        addAndMakeVisible(c);
    alignText_ = S("Mikrofon + inbyggd: välj Blanda och tryck Auto-justera medan du spelar.");
    alignColour_ = palette().secondaryInk;
    setSize(380, 440);
    timerCallback();
    startTimerHz(20);
}

void InputPanel::timerCallback() {
    auto& in = proc_.engine().input();
    if (auto r = in.analyseCapture()) {
        if (r->correlation < 0.3) {
            alignText_ = S("Osäkert – spela starkare och kontrollera att båda ingångarna har signal.");
            alignColour_ = palette().danger;
        } else {
            in.apply(*r);
            alignText_ = S("Justerat: ") + juce::String(std::abs(r->lagSamples) / in.sampleRate() * 1000.0, 2).replaceCharacter('.', ',')
                       + " ms" + (r->invert ? S(", ingång 2 vänd") : juce::String());
            alignColour_ = r->correlation < 0.6 ? palette().warn : palette().good;
        }
    }
    const int mode = in.mode.load();
    src1_.setToggleState(mode == 0, juce::dontSendNotification);
    src2_.setToggleState(mode == 1, juce::dontSendNotification);
    srcBlend_.setToggleState(mode == 2, juce::dontSendNotification);
    inv1_.setToggleState(in.invert1.load(), juce::dontSendNotification);
    inv2_.setToggleState(in.invert2.load(), juce::dontSendNotification);
    const bool blend = mode == 2;
    balance_.setEnabled(blend); balance_.setAlpha(blend ? 1.0f : 0.45f);
    align_.setEnabled(blend);   align_.setAlpha(blend ? 1.0f : 0.45f);
    autoAlign_.setEnabled(!in.isCapturing());
    for (auto* c : { &balance_, &align_, &inGain_, &outGain_ }) c->refresh();
    repaint();
}

void InputPanel::paint(juce::Graphics& g) {
    auto r = getLocalBounds().reduced(20, 16);
    g.setColour(palette().ink);
    g.setFont(font(17.0f, true));
    g.drawText(S("Ingång"), r.removeFromTop(24), juce::Justification::centredLeft);
    g.setColour(alignColour_);
    g.setFont(font(12.5f));
    g.drawFittedText(alignText_, autoAlign_.getBounds().translated(0, 40).withHeight(34), juce::Justification::topLeft, 2);
    drawCaption(g, S("Nivåer"), inGain_.getBounds().translated(0, -26).withHeight(18), palette().secondaryInk);
}

void InputPanel::resized() {
    auto r = getLocalBounds().reduced(20, 16);
    r.removeFromTop(32);
    auto src = r.removeFromTop(30);
    const int w = src.getWidth() / 3;
    src1_.setBounds(src.removeFromLeft(w).reduced(2, 0));
    src2_.setBounds(src.removeFromLeft(w).reduced(2, 0));
    srcBlend_.setBounds(src.reduced(2, 0));
    r.removeFromTop(12);
    balance_.setBounds(r.removeFromTop(ParamControl::kHeight));
    align_.setBounds(r.removeFromTop(ParamControl::kHeight));
    auto row = r.removeFromTop(32);
    inv1_.setBounds(row.removeFromLeft(56).reduced(0, 3));
    row.removeFromLeft(8);
    inv2_.setBounds(row.removeFromLeft(56).reduced(0, 3));
    row.removeFromLeft(12);
    autoAlign_.setBounds(row);
    r.removeFromTop(48);
    r.removeFromTop(22);
    inGain_.setBounds(r.removeFromTop(ParamControl::kHeight));
    outGain_.setBounds(r.removeFromTop(ParamControl::kHeight));
}

// ---------------------------------------------------------------------------
// HeaderBar
// ---------------------------------------------------------------------------
HeaderBar::HeaderBar() {
    addAndMakeVisible(input);
    addAndMakeVisible(more);
}

void HeaderBar::setTexts(const juce::String& title, const juce::String& subtitle) {
    if (title == title_ && subtitle == subtitle_) return;
    title_ = title;
    subtitle_ = subtitle;
    repaint();
}

void HeaderBar::paint(juce::Graphics& g) {
    auto r = getLocalBounds().reduced(80, 4);
    g.setColour(palette().ink);
    g.setFont(font(17.0f, true));
    g.drawText(title_, r.removeFromTop(r.getHeight() / 2 + 2), juce::Justification::centredBottom, true);
    g.setColour(palette().secondaryInk);
    g.setFont(font(12.5f));
    g.drawText(subtitle_, r.withTrimmedTop(2), juce::Justification::centredTop, true);
}

void HeaderBar::resized() {
    auto r = getLocalBounds();
    input.setBounds(r.removeFromLeft(44).withSizeKeepingCentre(44, 44));
    more.setBounds(r.removeFromRight(44).withSizeKeepingCentre(44, 44));
}

// ---------------------------------------------------------------------------
// BottomBar
// ---------------------------------------------------------------------------
BottomBar::BottomBar(GuitarFXProcessor& proc) : proc_(proc) {
    prev_.onClick = [this] { proc_.rig().selectPrevious(); };
    next_.onClick = [this] { proc_.rig().selectNext(); };
    addAndMakeVisible(prev_);
    addAndMakeVisible(next_);
}

void BottomBar::update(float in1, float in2, float out) {
    in1_ = juce::jmax(in1, in1_ * 0.9f);
    in2_ = juce::jmax(in2, in2_ * 0.9f);
    out_ = juce::jmax(out, out_ * 0.9f);
    repaint();
}

void BottomBar::paint(juce::Graphics& g) {
    const auto& p = palette();
    drawCard(g, getLocalBounds().toFloat().reduced(2.0f), 20.0f);

    // Mätare.
    auto m = meters_;
    const float levels[3] = { in1_, in2_, out_ };
    const char* labels[3] = { "IN 1", "IN 2", "UT" };
    const int rowH = m.getHeight() / 3;
    for (int i = 0; i < 3; ++i) {
        auto row = m.removeFromTop(rowH);
        drawCaption(g, labels[i], row.removeFromLeft(38), p.secondaryInk);
        auto bar = row.withSizeKeepingCentre(row.getWidth(), 5).toFloat();
        g.setColour(p.fill);
        g.fillRoundedRectangle(bar, 2.5f);
        const float db = juce::Decibels::gainToDecibels(levels[i], -100.0f);
        const float norm = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
        if (norm > 0.0f) {
            g.setColour(levelColour(levels[i]));
            g.fillRoundedRectangle(bar.withWidth(juce::jmax(5.0f, bar.getWidth() * norm)), 2.5f);
        }
    }

    // Aktivt preset i mitten.
    auto& rig = proc_.rig();
    const int a = rig.activePreset();
    auto c = centre_;
    drawCaption(g, a < 0 ? S("Inget preset") : S("Preset ") + juce::String(a + 1) + S(" av ") + juce::String(rig.presets().size()),
                c.removeFromTop(c.getHeight() / 2).withTrimmedTop(6), a < 0 ? p.secondaryInk : p.accent, juce::Justification::centredBottom);
    g.setColour(p.ink);
    g.setFont(font(17.0f, true));
    g.drawText(a < 0 ? S("Grundljud") : u8(rig.activeName()), c.withTrimmedBottom(4), juce::Justification::centredTop, true);

    // Statuschips till höger.
    auto chips = chips_;
    auto chip = [&](const juce::String& title, const juce::String& value, juce::Colour valueColour) {
        auto r = chips.removeFromRight(title.length() > 4 ? 92 : 78);
        chips.removeFromRight(8);
        g.setColour(p.subtle);
        g.fillRoundedRectangle(r.toFloat().reduced(0, 10), 10.0f);
        auto inner = r.reduced(12, 14);
        drawCaption(g, title, inner.removeFromTop(inner.getHeight() / 2), p.secondaryInk);
        g.setColour(valueColour);
        g.setFont(font(13.5f, true));
        g.drawText(value, inner, juce::Justification::centredLeft, false);
    };
    const double cpu = proc_.cpuLoad() * 100.0;
    const float gr = proc_.engine().limiterGainReductionDb();
    const auto midi = proc_.lastMidiDescription();
    if (midi.isNotEmpty()) chip("MIDI", midi, p.ink);
    chip("LIMITER", juce::String(gr, 1).replaceCharacter('.', ',') + " dB", gr < -0.5f ? p.warn : p.ink);
    chip("CPU", juce::String(juce::roundToInt(cpu)) + " %", cpu > 70 ? p.danger : cpu > 50 ? p.warn : p.ink);
}

void BottomBar::resized() {
    auto r = getLocalBounds().reduced(18, 0);
    meters_ = r.removeFromLeft(juce::jmin(240, r.getWidth() / 4)).reduced(0, 12);
    chips_ = r.removeFromRight(juce::jmin(300, r.getWidth() / 3));
    auto mid = r.withSizeKeepingCentre(juce::jmin(r.getWidth(), 360), r.getHeight());
    prev_.setBounds(mid.removeFromLeft(46).withSizeKeepingCentre(46, 46));
    next_.setBounds(mid.removeFromRight(46).withSizeKeepingCentre(46, 46));
    centre_ = mid;
}

} // namespace ui
