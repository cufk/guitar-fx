#include "Widgets.h"

namespace ui {

namespace {
juce::String S(const char* utf8) { return juce::String::fromUTF8(utf8); }

void drawCaption(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> r, juce::Colour c,
                 juce::Justification j = juce::Justification::centredLeft) {
    g.setColour(c);
    g.setFont(captionFont());
    g.drawText(upper(text), r, j, true);
}

float textWidth(const juce::Font& f, const juce::String& t) {
    return juce::GlyphArrangement::getStringWidth(f, t);
}

float levelNorm(float lvl) {
    return juce::jlimit(0.0f, 1.0f, (juce::Decibels::gainToDecibels(lvl, -100.0f) + 60.0f) / 60.0f);
}

juce::String minusSign(juce::String s) { return s.replaceCharacter('-', static_cast<juce::juce_wchar>(0x2212)); }
} // namespace

// ---------------------------------------------------------------------------
// IconButton
// ---------------------------------------------------------------------------
IconButton::IconButton(icons::Symbol s, const juce::String& tooltip, bool filled)
    : juce::Button(tooltip), symbol_(s), filled_(filled) {
    setTooltip(tooltip);
    setWantsKeyboardFocus(false);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void IconButton::paintButton(juce::Graphics& g, bool over, bool down) {
    const auto& p = palette();
    auto r = getLocalBounds().toFloat();
    const float d = juce::jmin(r.getWidth(), r.getHeight()) - 2.0f;
    auto circle = r.withSizeKeepingCentre(d, d);
    if (filled_ || over || down) {
        g.setColour(getToggleState() ? p.ink : down ? p.tileStrong : (filled_ ? (over ? p.tileStrong : p.tile) : p.tile));
        g.fillEllipse(circle);
    } else if (getToggleState()) {
        g.setColour(p.ink);
        g.fillEllipse(circle);
    }
    const auto ink = !isEnabled() ? p.tertiaryInk : getToggleState() ? p.inkInverse : p.ink;
    icons::draw(g, symbol_, circle.reduced(d * 0.3f), ink, juce::jmax(1.6f, d * 0.045f));
}

// ---------------------------------------------------------------------------
// Pill
// ---------------------------------------------------------------------------
Pill::Pill(const juce::String& label, bool toggle) : juce::Button(label) {
    setClickingTogglesState(toggle);
    setWantsKeyboardFocus(false);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void Pill::paintButton(juce::Graphics& g, bool over, bool down) {
    const auto& p = palette();
    const bool on = getToggleState();
    auto r = getLocalBounds().toFloat();
    auto fill = on ? p.ink.interpolatedWith(p.background, down ? 0.15f : 0.0f)
                   : (down || over ? p.tileStrong : p.tile);
    if (on && !onFill.isTransparent()) fill = onFill.interpolatedWith(p.background, down ? 0.15f : 0.0f);
    if (!on && !offFill.isTransparent()) fill = offFill.overlaidWith(p.ink.withAlpha(down ? 0.08f : over ? 0.04f : 0.0f));
    g.setColour(fill);
    g.fillRoundedRectangle(r, r.getHeight() * 0.5f);

    auto ink = !isEnabled() ? p.tertiaryInk : on ? p.inkInverse : p.ink;
    if (isEnabled() && on && !onText.isTransparent()) ink = onText;
    if (isEnabled() && !on && !offText.isTransparent()) ink = offText;
    const auto f = font(r.getHeight() >= 44.0f ? 15.0f : 13.5f, Weight::SemiBold);
    const juce::String label = getButtonText();
    float w = textWidth(f, label);
    const auto pf = font(f.getHeight(), Weight::Regular);
    const float pw = prefix.isNotEmpty() ? textWidth(pf, prefix) + 10.0f : 0.0f;
    const float sw = showSymbol ? 22.0f : 0.0f;
    const float total = juce::jmin(r.getWidth() - 28.0f, w + pw + sw);
    float x = r.getCentreX() - total * 0.5f;
    if (showSymbol) {
        icons::draw(g, symbol, { x, r.getCentreY() - 7.0f, 14.0f, 14.0f }, ink, 1.7f);
        x += sw;
    }
    if (prefix.isNotEmpty()) {
        g.setColour(on ? p.inkInverse.withAlpha(0.6f) : p.secondaryInk);
        g.setFont(pf);
        g.drawText(prefix, juce::Rectangle<float>(x, r.getY(), pw, r.getHeight()), juce::Justification::centredLeft, false);
        x += pw;
    }
    g.setColour(ink);
    g.setFont(f);
    g.drawText(label, juce::Rectangle<float>(x, r.getY(), r.getRight() - 14.0f - x, r.getHeight()), juce::Justification::centredLeft, true);
}

// ---------------------------------------------------------------------------
// Switch
// ---------------------------------------------------------------------------
Switch::Switch() : juce::Button({}) {
    setClickingTogglesState(true);
    setWantsKeyboardFocus(false);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void Switch::timerCallback() {
    const float target = getToggleState() ? 1.0f : 0.0f;
    pos_ += (target - pos_) * 0.35f;
    if (std::abs(target - pos_) < 0.01f) { pos_ = target; stopTimer(); }
    repaint();
}

void Switch::paintButton(juce::Graphics& g, bool over, bool) {
    const auto& p = palette();
    const float target = getToggleState() ? 1.0f : 0.0f;
    if (std::abs(target - pos_) > 0.01f && !isTimerRunning()) startTimerHz(60);
    auto r = getLocalBounds().toFloat().withSizeKeepingCentre(56.0f, 32.0f);
    g.setColour((over ? p.tileStrong : p.tile).interpolatedWith(p.ink, pos_));
    g.fillRoundedRectangle(r, 16.0f);
    const float kd = 24.0f;
    const float kx = r.getX() + 4.0f + pos_ * (r.getWidth() - kd - 8.0f);
    g.setColour(p.inkInverse.interpolatedWith(p.background, 0.0f));
    g.setColour(pos_ > 0.5f ? p.inkInverse : p.background);
    g.fillEllipse(kx, r.getCentreY() - kd * 0.5f, kd, kd);
}

// ---------------------------------------------------------------------------
// KnobTile
// ---------------------------------------------------------------------------
KnobTile::KnobTile(const gfx::ParamSpec& spec) : spec_(spec) {
    if (!spec_.choices.empty()) {
        for (size_t i = 0; i < spec_.choices.size(); ++i) {
            auto c = std::make_unique<Pill>(u8(spec_.choices[i]));
            c->setRadioGroupId(1);
            c->onClick = [this, i] { spec_.param->set(static_cast<float>(i)); };
            addAndMakeVisible(*c);
            choices_.push_back(std::move(c));
        }
    } else {
        knob_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knob_.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        knob_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        knob_.setRange(spec_.param->min(), spec_.param->max(), spec_.step);
        if (spec_.logScale && spec_.param->min() > 0.0f)
            knob_.setSkewFactorFromMidPoint(std::sqrt(static_cast<double>(spec_.param->min()) * spec_.param->max()));
        knob_.setMouseDragSensitivity(240);
        knob_.setDoubleClickReturnValue(true, spec_.defaultValue);
        knob_.setWantsKeyboardFocus(false);
        knob_.setScrollWheelEnabled(true);
        knob_.setTooltip(S("Dra upp/ned · dubbelklicka för standardvärdet (") + formatValue(spec_, spec_.defaultValue) + ")");
        knob_.onValueChange = [this] { spec_.param->set(static_cast<float>(knob_.getValue())); repaint(); };
        addAndMakeVisible(knob_);
    }
    refresh();
}

void KnobTile::refresh() {
    const float v = spec_.param->get();
    if (!choices_.empty()) {
        const int idx = juce::roundToInt(v);
        for (size_t i = 0; i < choices_.size(); ++i)
            choices_[i]->setToggleState(static_cast<int>(i) == idx, juce::dontSendNotification);
    } else if (!knob_.isMouseButtonDown() && std::abs(knob_.getValue() - v) > 1e-6) {
        knob_.setValue(v, juce::dontSendNotification);
        repaint();
    }
}

void KnobTile::paint(juce::Graphics& g) {
    const auto& p = palette();
    drawTile(g, getLocalBounds().toFloat(), 18.0f, p.tile);
    auto bottom = getLocalBounds().reduced(16, 12).removeFromBottom(22);
    g.setColour(p.ink);
    g.setFont(font(14.5f, Weight::Regular));
    g.drawText(u8(spec_.label), bottom, juce::Justification::centredLeft, true);
    if (choices_.empty()) {
        const float v = spec_.param->get();
        const bool changed = std::abs(v - spec_.defaultValue) > spec_.step * 0.5f;
        g.setColour(changed || knob_.isMouseButtonDown() ? p.ink : p.secondaryInk);
        g.setFont(font(13.5f, Weight::SemiBold));
        g.drawText(minusSign(formatValue(spec_, v)), bottom, juce::Justification::centredRight, false);
    }
}

void KnobTile::resized() {
    auto r = getLocalBounds().reduced(16, 14);
    r.removeFromBottom(30);
    if (!choices_.empty()) {
        const int h = juce::jmin(38, (r.getHeight() - 8 * (static_cast<int>(choices_.size()) - 1)) / static_cast<int>(choices_.size()));
        auto col = r.withSizeKeepingCentre(r.getWidth(), static_cast<int>(choices_.size()) * (h + 8) - 8);
        for (auto& c : choices_) { c->setBounds(col.removeFromTop(h)); col.removeFromTop(8); }
    } else {
        const int d = juce::jmin(r.getWidth(), r.getHeight());
        knob_.setBounds(r.withSizeKeepingCentre(d, d));
    }
}

// ---------------------------------------------------------------------------
// EffectGrid
// ---------------------------------------------------------------------------
EffectGrid::EffectGrid(GuitarFXProcessor& proc, std::function<void(int)> onSelect)
    : proc_(proc), onSelect_(std::move(onSelect)) {}

juce::Rectangle<int> EffectGrid::cell(int i) const {
    const int n = static_cast<int>(proc_.rig().effects().size());
    const int cols = kGridColumns, gap = kGridGap;
    const int rows = juce::jmax(1, (n + cols - 1) / cols);
    const int w = (getWidth() - gap * (cols - 1)) / cols, h = (getHeight() - gap * (rows - 1)) / rows;
    return { (i % cols) * (w + gap), (i / cols) * (h + gap), w, h };
}

int EffectGrid::cellAt(juce::Point<int> pt) const {
    const int n = static_cast<int>(proc_.rig().effects().size());
    for (int i = 0; i < n; ++i) if (cell(i).contains(pt)) return i;
    return -1;
}

void EffectGrid::paint(juce::Graphics& g) {
    const auto& p = palette();
    const auto& effects = proc_.rig().effects();
    for (int i = 0; i < static_cast<int>(effects.size()); ++i) {
        const auto& e = effects[size_t(i)];
        const bool sel = i == selected_;
        const bool on = proc_.engine().chain().isEnabled(e.slot);
        auto r = cell(i).toFloat();
        drawTile(g, r, 16.0f, sel ? p.ink : (hover_ == i ? p.tileStrong : p.tile));
        const auto ink = sel ? p.inkInverse : p.ink;
        auto inner = r.reduced(14.0f, 12.0f);
        icons::drawEffect(g, e.id, inner.removeFromTop(20.0f).removeFromLeft(20.0f), sel ? ink : (on ? p.ink : p.tertiaryInk));
        // Prick uppe till höger: fylld = på.
        auto dot = juce::Rectangle<float>(r.getRight() - 24.0f, r.getY() + 14.0f, 10.0f, 10.0f);
        g.setColour(ink.withAlpha(on ? 1.0f : 0.45f));
        if (on) g.fillEllipse(dot); else g.drawEllipse(dot.reduced(0.75f), 1.5f);
        g.setColour(ink);
        g.setFont(font(14.5f, Weight::SemiBold));
        g.drawText(u8(e.name), inner.toNearestInt(), juce::Justification::bottomLeft, true);
        if (proc_.rig().activeIncludes(e.id)) {
            g.setColour(sel ? p.inkInverse.withAlpha(0.65f) : p.secondaryInk);
            g.setFont(captionFont());
            g.drawText("PRESET", inner.toNearestInt(), juce::Justification::bottomRight, false);
        }
    }
}

void EffectGrid::mouseUp(const juce::MouseEvent& e) {
    const int i = cellAt(e.getPosition());
    if (i >= 0 && e.mouseWasClicked()) { selected_ = i; repaint(); if (onSelect_) onSelect_(i); }
}

void EffectGrid::mouseMove(const juce::MouseEvent& e) {
    const int i = cellAt(e.getPosition());
    setMouseCursor(i >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    if (i != hover_) { hover_ = i; repaint(); }
}

// ---------------------------------------------------------------------------
// EffectPanel
// ---------------------------------------------------------------------------
EffectPanel::EffectPanel(GuitarFXProcessor& proc) : proc_(proc) {
    switch_.onClick = [this] {
        if (index_ >= 0) proc_.engine().chain().setEnabled(proc_.rig().effects()[size_t(index_)].slot, switch_.getToggleState());
    };
    presetPill_.setTooltip(S("Ta med effekten i det aktiva presetet. Av = effekten följer grundljudet."));
    presetPill_.onClick = [this] {
        if (index_ >= 0) proc_.rig().setIncludedInActive(proc_.rig().effects()[size_t(index_)].id, presetPill_.getToggleState());
    };
    addAndMakeVisible(switch_);
    addChildComponent(presetPill_);
    loadIr_ = std::make_unique<Pill>(S("Ladda IR…"), false);
    builtInIr_ = std::make_unique<Pill>(S("Inbyggd"), false);
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
    show(0);
}

void EffectPanel::show(int i) {
    const auto& effects = proc_.rig().effects();
    if (i < 0 || i >= static_cast<int>(effects.size())) return;
    index_ = i;
    tiles_.clear();
    for (const auto& p : effects[size_t(i)].params) {
        tiles_.push_back(std::make_unique<KnobTile>(p));
        addAndMakeVisible(*tiles_.back());
    }
    const bool cab = effects[size_t(i)].id == "cabinet";
    loadIr_->setVisible(cab);
    builtInIr_->setVisible(cab);
    refresh();
    resized();
    repaint();
}

void EffectPanel::refresh() {
    if (index_ < 0) return;
    const auto& e = proc_.rig().effects()[size_t(index_)];
    auto& rig = proc_.rig();
    const bool on = proc_.engine().chain().isEnabled(e.slot);
    if (switch_.getToggleState() != on) switch_.setToggleState(on, juce::dontSendNotification);
    const bool presetActive = rig.activePreset() >= 0;
    if (presetPill_.isVisible() != presetActive) { presetPill_.setVisible(presetActive); resized(); }
    presetPill_.setToggleState(presetActive && rig.activeIncludes(e.id), juce::dontSendNotification);
    activity_ = (on && e.activity) ? e.activity() : 0.0f;
    for (auto& t : tiles_) t->refresh();
    repaint(0, 0, getWidth(), 96);
}

void EffectPanel::paint(juce::Graphics& g) {
    if (index_ < 0) return;
    const auto& p = palette();
    const auto& e = proc_.rig().effects()[size_t(index_)];
    auto& rig = proc_.rig();
    const bool on = switch_.getToggleState();
    auto head = getLocalBounds().removeFromTop(84);
    auto text = head.withTrimmedRight(getWidth() - presetPill_.getX() + 16);
    if (!presetPill_.isVisible()) text = head.withTrimmedRight(getWidth() - (loadIr_->isVisible() ? loadIr_->getX() : switch_.getX()) + 16);

    g.setColour(p.ink);
    g.setFont(font(30.0f, Weight::Bold));
    g.drawText(u8(e.name), text.removeFromTop(48).withTrimmedTop(4), juce::Justification::bottomLeft, true);

    juce::String status;
    if (rig.activePreset() < 0) status = on ? S("På i grundljudet") : S("Av");
    else if (rig.activeIncludes(e.id)) status = S("Sparas i presetet ") + u8(rig.activeName()) + (on ? "" : S(" · av"));
    else status = on ? S("På · följer grundljudet") : S("Av · följer grundljudet");
    if (e.id == "cabinet") status << S(" · ") << u8(proc_.cabinet().impulseResponseName());
    if (activity_ < -0.5f) status << S(" · ") << juce::String(juce::roundToInt(-activity_)) << S(" dB kompression");
    g.setColour(irStatus_.isNotEmpty() ? p.danger : p.secondaryInk);
    g.setFont(font(14.5f));
    g.drawText(irStatus_.isNotEmpty() ? irStatus_ : status, text.withTrimmedTop(4), juce::Justification::topLeft, true);
}

void EffectPanel::resized() {
    auto r = getLocalBounds();
    auto head = r.removeFromTop(84);
    switch_.setBounds(head.removeFromRight(60).withSizeKeepingCentre(60, 40));
    head.removeFromRight(12);
    if (presetPill_.isVisible()) {
        presetPill_.setBounds(head.removeFromRight(118).withSizeKeepingCentre(118, 44));
        head.removeFromRight(10);
    }
    if (loadIr_->isVisible()) {
        builtInIr_->setBounds(head.removeFromRight(104).withSizeKeepingCentre(104, 44));
        head.removeFromRight(8);
        loadIr_->setBounds(head.removeFromRight(118).withSizeKeepingCentre(118, 44));
    }
    r.removeFromTop(8);

    const int n = static_cast<int>(tiles_.size());
    if (n == 0) return;
    // Fast rutnät med fyra kolumner (samma som effektrutorna ovanför): två rattar tar
    // två kolumner och resten lämnas tomt – rutorna sträcks aldrig ut.
    constexpr int cols = kGridColumns;
    const int rows = (n + cols - 1) / cols;
    const int gap = kGridGap;
    const int w = (r.getWidth() - gap * (cols - 1)) / cols;
    const int h = juce::jmin((r.getHeight() - gap * (rows - 1)) / rows, w + 40);
    for (int i = 0; i < n; ++i)
        tiles_[size_t(i)]->setBounds(r.getX() + (i % cols) * (w + gap), r.getY() + (i / cols) * (h + gap), w, h);
}

// ---------------------------------------------------------------------------
// StageView
// ---------------------------------------------------------------------------
StageView::StageView(GuitarFXProcessor& proc) : proc_(proc) {
    prev_.onClick = [this] { proc_.rig().selectPrevious(); };
    next_.onClick = [this] { proc_.rig().selectNext(); };
    addAndMakeVisible(prev_);
    addAndMakeVisible(next_);
}

void StageView::update(float in1, float in2, float out, const juce::String& latency) {
    const float v[3] = { in1, in2, out };
    for (int i = 0; i < 3; ++i) level_[i] = juce::jmax(v[i], level_[i] * 0.88f);
    const float target = levelNorm(out);
    ring_ += (target - ring_) * (target > ring_ ? 0.6f : 0.12f);
    if (target >= hold_) { hold_ = target; holdTicks_ = 40; }
    else if (--holdTicks_ <= 0) hold_ = juce::jmax(0.0f, hold_ - 0.01f);
    latency_ = latency;
    repaint();
}

void StageView::paint(juce::Graphics& g) {
    const auto& p = palette();
    auto& rig = proc_.rig();
    const int a = rig.activePreset();

    // Ring av streck: tända streck = utnivå (−60..0 dB), ett längre streck = topphållning.
    constexpr int kTicks = 72;
    const auto c = ringArea_.getCentre();
    const float outer = ringArea_.getWidth() * 0.5f;
    const int lit = juce::roundToInt(ring_ * kTicks);
    const int holdTick = juce::roundToInt(hold_ * kTicks);
    for (int i = 0; i < kTicks; ++i) {
        const float ang = juce::MathConstants<float>::twoPi * (static_cast<float>(i) / kTicks) + juce::MathConstants<float>::pi; // start nertill
        const bool on = i < lit;
        const bool peak = i == holdTick - 1 && holdTick > 1;
        const float len = on || peak ? outer * 0.16f : outer * 0.10f;
        const float r0 = outer - len, r1 = outer;
        juce::Line<float> line(c.x + r0 * std::sin(ang), c.y - r0 * std::cos(ang), c.x + r1 * std::sin(ang), c.y - r1 * std::cos(ang));
        g.setColour(on || peak ? (levelNorm(level_[2]) > 0.985f && on ? p.danger : p.ink) : p.line);
        g.drawLine(line, on || peak ? 3.0f : 1.4f);
    }

    // Stort nummer i mitten.
    const auto num = juce::String(a + 1);
    g.setColour(p.ink);
    g.setFont(font(outer * 0.78f, Weight::Light));
    g.drawText(num, ringArea_.withSizeKeepingCentre(outer * 1.4f, outer).translated(0, -outer * 0.06f), juce::Justification::centred, false);
    g.setColour(p.secondaryInk);
    g.setFont(captionFont());
    g.drawText(a < 0 ? S("GRUNDLJUD") : S("PRESET ") + juce::String(a + 1) + S(" AV ") + juce::String(rig.presets().size()),
               ringArea_.withSizeKeepingCentre(outer, 20.0f).translated(0, outer * 0.42f), juce::Justification::centred, false);

    // Namn under ringen.
    g.setColour(p.ink);
    g.setFont(font(26.0f, Weight::SemiBold));
    g.drawText(a < 0 ? S("Grundljud") : u8(rig.activeName()), nameArea_, juce::Justification::centred, true);

    // Ingångsmätare och status.
    auto m = metersArea_;
    const char* labels[2] = { "In 1", "In 2" };
    for (int i = 0; i < 2; ++i) {
        auto row = m.removeFromTop(m.getHeight() / (2 - i));
        drawCaption(g, labels[i], row.removeFromLeft(40), p.secondaryInk);
        auto bar = row.withSizeKeepingCentre(row.getWidth(), 4).toFloat();
        g.setColour(p.line);
        g.fillRoundedRectangle(bar, 2.0f);
        const float n = levelNorm(level_[i]);
        if (n > 0.0f) {
            g.setColour(n > 0.985f ? p.danger : p.ink);
            g.fillRoundedRectangle(bar.withWidth(juce::jmax(4.0f, bar.getWidth() * n)), 2.0f);
        }
    }
    const double cpu = proc_.cpuLoad() * 100.0;
    const float gr = proc_.engine().limiterGainReductionDb();
    juce::String status = S("CPU ") + juce::String(juce::roundToInt(cpu)) + S(" %   ·   Limiter ")
                        + (gr < -0.05f ? minusSign(juce::String(gr, 1).replaceCharacter('.', ',')) + " dB" : S("vilar"));
    if (latency_.isNotEmpty()) status << S("   ·   Latens ") << latency_;
    const auto midi = proc_.lastMidiDescription();
    if (midi.isNotEmpty()) status << S("   ·   MIDI ") << midi;
    g.setColour(cpu > 70 ? p.danger : p.tertiaryInk);
    g.setFont(font(12.5f));
    g.drawText(status, statusArea_, juce::Justification::centredLeft, true);
}

void StageView::resized() {
    auto r = getLocalBounds();
    statusArea_ = r.removeFromBottom(22);
    r.removeFromBottom(10);
    metersArea_ = r.removeFromBottom(40);
    r.removeFromBottom(28);
    auto nav = r.removeFromBottom(52);
    r.removeFromBottom(20);
    const float d = static_cast<float>(juce::jmin(r.getWidth() - 40, r.getHeight()));
    ringArea_ = r.toFloat().withSizeKeepingCentre(d, d);
    auto navC = nav.withSizeKeepingCentre(juce::jmin(nav.getWidth(), 380), 52);
    prev_.setBounds(navC.removeFromLeft(52));
    next_.setBounds(navC.removeFromRight(52));
    nameArea_ = navC.reduced(10, 0);
}

// ---------------------------------------------------------------------------
// PresetBar
// ---------------------------------------------------------------------------
PresetBar::PresetBar(GuitarFXProcessor& proc) : proc_(proc) {
    add_.onClick = [this] {
        auto& rig = proc_.rig();
        const int i = rig.addPreset("Preset " + std::to_string(rig.presets().size() + 1));
        rig.activate(i);
        rename(i);
    };
    addAndMakeVisible(add_);
    addAndMakeVisible(analyse);
    refresh();
}

void PresetBar::rebuild() {
    auto& rig = proc_.rig();
    pills_.clear();
    const int n = static_cast<int>(rig.presets().size());
    for (int t = -1; t < n; ++t) {
        auto pill = std::make_unique<Pill>(t < 0 ? S("Grundljud") : u8(rig.presets()[size_t(t)].name), false);
        pill->prefix = juce::String(t + 1);
        pill->onClick = [this, t] {
            if (t >= 0 && isMenuGesture()) return;   // menyn öppnas i mouseUp
            if (t < 0) proc_.rig().activate(-1); else proc_.rig().select(t);
        };
        if (t >= 0) {
            pill->setTooltip(S("Tryck = aktivera · högerklick = byt namn, flytta, ta bort"));
            pill->addMouseListener(this, false);
            pill->getProperties().set("preset", t);
        }
        addAndMakeVisible(*pill);
        pills_.push_back(std::move(pill));
    }
    lastCount_ = rig.presets().size();
    resized();
}

void PresetBar::refresh() {
    auto& rig = proc_.rig();
    if (rig.revision() == lastRevision_) return;
    lastRevision_ = rig.revision();
    bool namesChanged = rig.presets().size() != lastCount_;
    for (size_t i = 0; !namesChanged && i < rig.presets().size(); ++i)
        namesChanged = pills_[i + 1]->getButtonText() != u8(rig.presets()[i].name);
    if (namesChanged) rebuild();
    for (size_t i = 0; i < pills_.size(); ++i)
        pills_[i]->setToggleState(static_cast<int>(i) - 1 == rig.activePreset(), juce::dontSendNotification);
}

void PresetBar::paint(juce::Graphics&) {}

void PresetBar::resized() {
    auto r = getLocalBounds();
    add_.setBounds(r.removeFromRight(r.getHeight()));
    r.removeFromRight(10);
    analyse.setBounds(r.removeFromRight(r.getHeight()));
    r.removeFromRight(12);
    if (pills_.empty()) return;
    const auto f = font(15.0f, Weight::SemiBold);
    std::vector<float> widths;
    float total = 0.0f;
    for (auto& p : pills_) {
        const float w = juce::jmax(132.0f, textWidth(f, p->getButtonText()) + textWidth(f, p->prefix) + 60.0f);
        widths.push_back(w);
        total += w;
    }
    const float gap = 10.0f;
    const float avail = static_cast<float>(r.getWidth()) - gap * static_cast<float>(pills_.size() - 1);
    const float scale = total > avail ? avail / total : 1.0f;
    float x = static_cast<float>(r.getX());
    for (size_t i = 0; i < pills_.size(); ++i) {
        const float w = widths[i] * scale;
        pills_[i]->setBounds(juce::roundToInt(x), r.getY(), juce::roundToInt(w), r.getHeight());
        x += w + gap;
    }
}

void PresetBar::showMenu(int i) {
    auto& rig = proc_.rig();
    if (i < 0 || i >= static_cast<int>(rig.presets().size())) return;
    const int n = static_cast<int>(rig.presets().size());
    juce::PopupMenu m;
    m.addSectionHeader(u8(rig.presets()[size_t(i)].name));
    m.addItem(1, S("Byt namn…"));
    m.addItem(2, "Duplicera");
    m.addItem(3, S("Flytta åt vänster"), i > 0);
    m.addItem(4, S("Flytta åt höger"), i < n - 1);
    m.addSeparator();
    m.addItem(5, S("Ta bort…"));
    juce::Component::SafePointer<PresetBar> safe(this);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(pills_[size_t(i + 1)].get()), [safe, i](int r) {
        if (!safe) return;
        auto& model = safe->proc_.rig();
        switch (r) {
            case 1: safe->rename(i); break;
            case 2: model.duplicatePreset(i); break;
            case 3: model.movePreset(i, i - 1); break;
            case 4: model.movePreset(i, i + 1); break;
            case 5:
                juce::AlertWindow::showAsync(
                    juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::NoIcon)
                        .withTitle(S("Ta bort ") + u8(model.presets()[size_t(i)].name) + "?")
                        .withMessage(S("Presetet tas bort. Grundljudet och andra presets påverkas inte."))
                        .withButton("Ta bort", 1).withButton("Avbryt", 0).withAssociatedComponent(safe.getComponent()),
                    [safe, i](int res) { if (safe && res == 1) safe->proc_.rig().removePreset(i); });
                break;
            default: break;
        }
    });
}

void PresetBar::rename(int i) {
    auto& rig = proc_.rig();
    if (i < 0 || i >= static_cast<int>(rig.presets().size())) return;
    auto* w = new juce::AlertWindow(S("Namn på preset"), S("Ge presetet ett namn du känner igen på scen."),
                                    juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor("name", u8(rig.presets()[size_t(i)].name));
    w->addButton("Spara", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Avbryt", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<PresetBar> safe(this);
    w->enterModalState(true, juce::ModalCallbackFunction::create([safe, w, i](int r) {
        if (!safe || r != 1) return;
        const auto name = w->getTextEditorContents("name").trim();
        if (name.isNotEmpty()) safe->proc_.rig().renamePreset(i, name.toStdString());
    }), true);
}

// Högerklick / långt tryck på ett presetpiller öppnar menyn (via addMouseListener).
void PresetBar::mouseDown(const juce::MouseEvent& e) {
    downTime_ = juce::Time::getMillisecondCounter();
    popupDown_ = e.mods.isPopupMenu();
}

bool PresetBar::isMenuGesture() const {
    return popupDown_ || juce::Time::getMillisecondCounter() - downTime_ >= 500;
}

void PresetBar::mouseUp(const juce::MouseEvent& e) {
    if (e.eventComponent == this || e.mouseWasDraggedSinceMouseDown()) return;
    const auto v = e.eventComponent->getProperties()["preset"];
    if (!v.isVoid() && isMenuGesture()) showMenu(static_cast<int>(v));
    popupDown_ = false;
}

// ---------------------------------------------------------------------------
// InputPanel
// ---------------------------------------------------------------------------
InputPanel::InputPanel(GuitarFXProcessor& proc)
    : proc_(proc),
      balanceSpec_ { "balance", "Balans 1–2", "", &proc.engine().input().balance, 0.01f, 2, false, {}, 0.5f },
      alignSpec_   { "align", "Justering", "ms", &proc.engine().input().alignMs, 0.01f, 2, false, {}, 0.0f },
      inSpec_      { "in", "In-nivå", "dB", &proc.engine().inputGain().gainDb, 0.1f, 1, false, {}, 0.0f },
      outSpec_     { "out", "Ut-nivå", "dB", &proc.engine().outputGain().gainDb, 0.1f, 1, false, {}, 0.0f },
      src1_(S("Ingång 1")), src2_(S("Ingång 2")), srcBlend_(S("Blanda")),
      outStereo_(S("Stereo")), outMono_(S("Mono (summa)")), inv1_(S("Ø 1")), inv2_(S("Ø 2")),
      balance_(balanceSpec_), align_(alignSpec_), inGain_(inSpec_), outGain_(outSpec_) {
    auto& in = proc_.engine().input();
    int mode = 0;
    for (auto* c : { &src1_, &src2_, &srcBlend_ }) {
        c->setRadioGroupId(7);
        c->onClick = [&in, mode] { in.mode.store(mode); };
        ++mode;
    }
    outStereo_.setRadioGroupId(8);
    outMono_.setRadioGroupId(8);
    outStereo_.onClick = [this] { proc_.engine().monoOutput.store(false); };
    outMono_.onClick = [this] { proc_.engine().monoOutput.store(true); };
    outMono_.setTooltip(S("Samma signal på båda utgångarna – välj om bara en kanal går till mixern"));
    inv1_.onClick = [this] { proc_.engine().input().invert1.store(inv1_.getToggleState()); };
    inv2_.onClick = [this] { proc_.engine().input().invert2.store(inv2_.getToggleState()); };
    inv1_.setTooltip(S("Vänd polariteten på ingång 1"));
    inv2_.setTooltip(S("Vänd polariteten på ingång 2"));
    autoAlign_.onClick = [this] {
        proc_.engine().input().requestCapture();
        alignText_ = S("Spela nu – lyssnar på båda ingångarna…");
        alignWarning_ = false;
    };
    for (juce::Component* c : std::initializer_list<juce::Component*> { &src1_, &src2_, &srcBlend_, &outStereo_, &outMono_, &inv1_, &inv2_,
                                                                       &autoAlign_, &balance_, &align_, &inGain_, &outGain_ })
        addAndMakeVisible(c);
    alignText_ = S("Mikrofon + inbyggd: välj Blanda och tryck Auto-justera medan du spelar.");
    setSize(460, 720);
    timerCallback();
    startTimerHz(20);
}

void InputPanel::timerCallback() {
    auto& in = proc_.engine().input();
    if (auto r = in.analyseCapture()) {
        if (r->correlation < 0.3) {
            alignText_ = S("Osäkert – spela starkare och kontrollera att båda ingångarna har signal.");
            alignWarning_ = true;
        } else {
            in.apply(*r);
            alignText_ = S("Justerat: ") + juce::String(std::abs(r->lagSamples) / in.sampleRate() * 1000.0, 2).replaceCharacter('.', ',')
                       + " ms" + (r->invert ? S(", ingång 2 vänd") : juce::String())
                       + (r->correlation < 0.6 ? S(" (osäkert – lyssna)") : juce::String());
            alignWarning_ = false;
        }
    }
    const int mode = in.mode.load();
    src1_.setToggleState(mode == 0, juce::dontSendNotification);
    src2_.setToggleState(mode == 1, juce::dontSendNotification);
    srcBlend_.setToggleState(mode == 2, juce::dontSendNotification);
    const bool mono = proc_.engine().monoOutput.load();
    outStereo_.setToggleState(!mono, juce::dontSendNotification);
    outMono_.setToggleState(mono, juce::dontSendNotification);
    inv1_.setToggleState(in.invert1.load(), juce::dontSendNotification);
    inv2_.setToggleState(in.invert2.load(), juce::dontSendNotification);
    const bool blend = mode == 2;
    balance_.setEnabled(blend); balance_.setAlpha(blend ? 1.0f : 0.4f);
    align_.setEnabled(blend);   align_.setAlpha(blend ? 1.0f : 0.4f);
    autoAlign_.setEnabled(!in.isCapturing());
    for (auto* c : { &balance_, &align_, &inGain_, &outGain_ }) c->refresh();
    repaint();
}

void InputPanel::paint(juce::Graphics& g) {
    const auto& p = palette();
    auto r = getLocalBounds().reduced(28, 24);
    g.setColour(p.ink);
    g.setFont(font(26.0f, Weight::Bold));
    g.drawText(S("Ingång och utgång"), r.removeFromTop(32), juce::Justification::centredLeft);
    for (const auto& [text, y] : captions_)
        drawCaption(g, text, { 28, y, getWidth() - 56, 18 }, p.secondaryInk);
    g.setColour(alignWarning_ ? p.danger : p.secondaryInk);
    g.setFont(font(13.0f));
    g.drawFittedText(alignText_, alignTextArea_, juce::Justification::topLeft, 2);
}

void InputPanel::resized() {
    captions_.clear();
    auto r = getLocalBounds().reduced(28, 24);
    r.removeFromTop(48);
    auto caption = [&](const char* text) { captions_.push_back({ S(text), r.getY() }); r.removeFromTop(24); };
    auto segments = [&](std::initializer_list<Pill*> pills) {
        auto row = r.removeFromTop(44);
        const int gap = 8;
        const int w = (row.getWidth() - gap * (static_cast<int>(pills.size()) - 1)) / static_cast<int>(pills.size());
        for (auto* c : pills) { c->setBounds(row.removeFromLeft(w)); row.removeFromLeft(gap); }
    };
    auto knobs = [&](KnobTile& a, KnobTile& b) {
        auto row = r.removeFromTop(150);
        a.setBounds(row.removeFromLeft((row.getWidth() - 12) / 2));
        row.removeFromLeft(12);
        b.setBounds(row);
    };
    caption("Källa");
    segments({ &src1_, &src2_, &srcBlend_ });
    r.removeFromTop(14);
    knobs(balance_, align_);
    r.removeFromTop(12);
    auto row = r.removeFromTop(44);
    inv1_.setBounds(row.removeFromLeft(72));
    row.removeFromLeft(8);
    inv2_.setBounds(row.removeFromLeft(72));
    row.removeFromLeft(8);
    autoAlign_.setBounds(row);
    r.removeFromTop(8);
    alignTextArea_ = r.removeFromTop(36);
    r.removeFromTop(8);
    caption("Utgång");
    segments({ &outStereo_, &outMono_ });
    r.removeFromTop(18);
    caption("Nivåer");
    knobs(inGain_, outGain_);
}


// ---------------------------------------------------------------------------
// TunerStrip
// ---------------------------------------------------------------------------
const std::vector<Tuning>& tunings() {
    static const std::vector<Tuning> t {
        { "Standard",          { 40, 45, 50, 55, 59, 64 } },   // E A D G B E
        { "Drop D",            { 38, 45, 50, 55, 59, 64 } },   // D A D G B E
        { "Halvton ner",       { 39, 44, 49, 54, 58, 63 } },   // D# G# C# F# A# D#
        { "Helton ner",        { 38, 43, 48, 53, 57, 62 } },   // D G C F A D
        { "Drop C",            { 36, 43, 48, 53, 57, 62 } },   // C G C F A D
        { "DADGAD",            { 38, 45, 50, 55, 57, 62 } },
        { "Open G",            { 38, 43, 50, 55, 59, 62 } },   // D G D G B D
        { "Open D",            { 38, 45, 50, 54, 57, 62 } },   // D A D F# A D
        { "Open E",            { 40, 47, 52, 56, 59, 64 } },   // E B E G# B E
        { "Open C",            { 36, 43, 48, 55, 60, 64 } },   // C G C G C E
        { "Kromatisk",         {} },                           // bara närmaste ton
    };
    return t;
}

TunerStrip::TunerStrip(GuitarFXProcessor& proc) : proc_(proc) {
    mute_.setTooltip(S("Tysta utgången medan du stämmer (T / MIDI CC 105)"));
    tuning_.setTooltip(S("Stämning"));
    tuning_.onClick = [this] {
        juce::PopupMenu m;
        m.addSectionHeader(S("Stämning"));
        const auto& list = tunings();
        for (size_t i = 0; i < list.size(); ++i) {
            juce::String label = S(list[i].name);
            if (!list[i].strings.empty()) {
                juce::StringArray notes;
                for (int midi : list[i].strings) notes.add(gfx::noteFromHz(gfx::hzFromMidi(midi)).name);
                label << "   " << notes.joinIntoString(" ");
            }
            m.addItem(static_cast<int>(i) + 1, label, true, proc_.tuning.load() == static_cast<int>(i));
        }
        juce::Component::SafePointer<TunerStrip> safe(this);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&tuning_), [safe](int r) {
            if (safe && r > 0) safe->proc_.tuning.store(r - 1);
        });
    };
    mute_.onClick = [this] { proc_.engine().tuner().muteOutput.store(mute_.getToggleState()); repaint(); };
    reference_.setTooltip(S("Referenston för A4"));
    reference_.onClick = [this] {
        juce::PopupMenu m;
        m.addSectionHeader(S("Referens A4"));
        for (int f = 430; f <= 446; f += (f < 438 || f >= 442) ? 2 : 1)
            m.addItem(f, juce::String(f) + " Hz", true, juce::roundToInt(proc_.referenceA4.load()) == f);
        juce::Component::SafePointer<TunerStrip> safe(this);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&reference_), [safe](int r) {
            if (safe && r > 0) safe->proc_.referenceA4.store(static_cast<float>(r));
        });
    };
    addAndMakeVisible(mute_);
    addAndMakeVisible(tuning_);
    addAndMakeVisible(reference_);
    proc_.engine().tuner().active.store(true);   // lyssnar så länge fönstret är öppet
    startTimerHz(25);
}

TunerStrip::~TunerStrip() {
    // Stängs fönstret slutar stämapparaten lyssna – och en ev. tystad utgång släpps.
    proc_.engine().tuner().active.store(false);
}

void TunerStrip::toggleMute() {
    auto& t = proc_.engine().tuner();
    t.muteOutput.store(!t.muteOutput.load());
}

void TunerStrip::timerCallback() {
    auto& tap = proc_.engine().tuner();
    const double sr = proc_.engine().spec().sampleRate > 0 ? proc_.engine().spec().sampleRate : 48000.0;
    if (std::abs(sr - preparedRate_) > 1.0) {
        detector_.prepare(sr, static_cast<int>(sr * 0.085));   // ca 85 ms fönster
        window_.assign(static_cast<size_t>(detector_.windowSamples()), 0.0f);
        preparedRate_ = sr;
    }
    const bool muted = tap.muteOutput.load();
    mute_.setToggleState(muted, juce::dontSendNotification);
    // Pillren ska synas mot fältets bakgrund i båda lägena (tystad = svart fält).
    const auto& p = palette();
    for (auto* pill : { &mute_, &tuning_, &reference_ }) {
        pill->offFill = muted ? p.inkInverse.withAlpha(0.14f) : p.background;
        pill->offText = muted ? p.inkInverse : p.ink;
        pill->onFill  = muted ? p.inkInverse : p.ink;
        pill->onText  = muted ? p.ink : p.inkInverse;
    }
    mute_.setButtonText(muted ? S("Muted") : S("Mute"));
    const auto& list = tunings();
    const auto& tuning = list[size_t(juce::jlimit(0, static_cast<int>(list.size()) - 1, proc_.tuning.load()))];
    if (tuning_.getButtonText() != S(tuning.name)) { tuning_.setButtonText(S(tuning.name)); resized(); }
    const float referenceA4 = proc_.referenceA4.load();
    const auto refText = juce::String(juce::roundToInt(referenceA4)) + " Hz";
    if (reference_.getButtonText() != refText) reference_.setButtonText(refText);

    tap.latest(window_.data(), static_cast<int>(window_.size()));
    const auto r = detector_.detect(window_.data(), static_cast<int>(window_.size()));
    if (r.hz > 0.0f && r.confidence > 0.85f && r.level > 0.002f) {
        // Median av de senaste avläsningarna – stabil men inte trög.
        recent_.push_back(r.hz);
        if (recent_.size() > 5) recent_.erase(recent_.begin());
        auto sorted = recent_;
        std::sort(sorted.begin(), sorted.end());
        const float hz = sorted[sorted.size() / 2];
        const auto n = gfx::noteFromHz(hz, referenceA4);
        if (n.midi != midi_) shownCents_ = n.cents;   // ny ton: hoppa direkt
        hz_ = hz;
        midi_ = n.midi;
        cents_ = n.cents;
        holdTicks_ = 25;   // visa senaste ton ca 1 s efter att den tystnat
    } else if (holdTicks_ > 0 && --holdTicks_ == 0) {
        hz_ = 0.0f;
        recent_.clear();
    }
    shownCents_ += (cents_ - shownCents_) * 0.35f;

    nearest_ = -1;
    if (hz_ > 0.0f) {
        int bestDist = 1000;
        for (size_t i = 0; i < tuning.strings.size(); ++i) {
            const int dist = std::abs(midi_ - tuning.strings[i]);
            if (dist < bestDist) { bestDist = dist; nearest_ = static_cast<int>(i); }
        }
        if (bestDist > 2) nearest_ = -1;
    }
    repaint();
}

void TunerStrip::paint(juce::Graphics& g) {
    const auto& p = palette();
    const bool muted = proc_.engine().tuner().muteOutput.load();
    // Tystad utgång: hela fältet inverterat så att det syns på håll.
    const auto bg = muted ? p.ink : p.tile;
    const auto ink = muted ? p.inkInverse : p.ink;
    const auto dim = muted ? p.inkInverse.withAlpha(0.55f) : p.secondaryInk;
    const auto faint = muted ? p.inkInverse.withAlpha(0.25f) : p.line;
    drawTile(g, getLocalBounds().toFloat(), 16.0f, bg);

    const bool hasNote = hz_ > 0.0f;
    const bool inTune = hasNote && std::abs(cents_) < 3.0f;
    const auto n = gfx::noteFromHz(hz_, proc_.referenceA4.load());
    const auto& list = tunings();
    const auto& tuning = list[size_t(juce::jlimit(0, static_cast<int>(list.size()) - 1, proc_.tuning.load()))];

    // Ton.
    const float noteH = static_cast<float>(noteArea_.getHeight());
    g.setColour(hasNote ? ink : dim);
    const auto nf = font(noteH * 0.9f, Weight::Light);
    g.setFont(nf);
    const auto noteText = hasNote ? juce::String(n.name.c_str()) : S("–");
    g.drawText(noteText, noteArea_, juce::Justification::centredLeft, false);
    if (hasNote) {
        const float nw = textWidth(nf, noteText);
        g.setColour(dim);
        g.setFont(font(noteH * 0.28f));
        g.drawText(juce::String(n.octave), juce::Rectangle<float>(static_cast<float>(noteArea_.getX()) + nw + 3.0f,
                   static_cast<float>(noteArea_.getBottom()) - noteH * 0.42f, 30.0f, noteH * 0.32f), juce::Justification::centredLeft, false);
    }

    // Skala −50 … +50 cent, tänt streck = avvikelse (mitten markeras när strängen är stämd).
    constexpr int kTicks = 51;
    const auto sa = scaleArea_.toFloat();
    const float cy = sa.getCentreY();
    const int lit = juce::jlimit(0, kTicks - 1, juce::roundToInt((shownCents_ + 50.0f) / 100.0f * (kTicks - 1)));
    for (int i = 0; i < kTicks; ++i) {
        const float x = sa.getX() + sa.getWidth() * static_cast<float>(i) / (kTicks - 1);
        const bool centre = i == kTicks / 2;
        const bool major = centre || i % 5 == 0;
        const bool on = hasNote && i == lit;
        float h = major ? sa.getHeight() * 0.36f : sa.getHeight() * 0.2f;
        if (on || (centre && inTune)) h = sa.getHeight();
        g.setColour(on || (centre && inTune) ? ink : (centre ? dim : faint));
        g.fillRoundedRectangle(x - (on ? 2.5f : 0.75f), cy - h * 0.5f, on ? 5.0f : 1.5f, h, 1.0f);
    }

    // Cent och Hz (eller status).
    juce::String info, sub;
    if (!hasNote) { info = muted ? S("Utgång tystad") : S("Stämapparat"); sub = S("Spela en sträng"); }
    else if (inTune) { info = S("Stämd"); sub = juce::String(hz_, 1).replaceCharacter('.', ',') + " Hz"; }
    else {
        info = minusSign((cents_ > 0 ? "+" : "") + juce::String(juce::roundToInt(cents_))) + S(" cent");
        sub = juce::String(hz_, 1).replaceCharacter('.', ',') + S(" Hz · ") + (cents_ > 0 ? S("för högt") : S("för lågt"));
    }
    auto ia = infoArea_;
    g.setColour(ink);
    g.setFont(font(15.0f, Weight::SemiBold));
    g.drawText(info, ia.removeFromTop(ia.getHeight() / 2 + 1), juce::Justification::bottomLeft, true);
    g.setColour(dim);
    g.setFont(font(12.0f));
    g.drawText(sub, ia.withTrimmedTop(1), juce::Justification::topLeft, true);

    // Strängar i vald stämning: närmaste markeras med en fylld cirkel.
    const int ns = static_cast<int>(tuning.strings.size());
    auto st = stringsArea_.toFloat();
    if (ns == 0) {
        g.setColour(dim);
        g.setFont(font(12.5f));
        g.drawText(S("Kromatisk"), st, juce::Justification::centred, false);
    }
    const float cw = ns > 0 ? st.getWidth() / static_cast<float>(ns) : 0.0f;
    for (int i = 0; i < ns; ++i) {
        const auto name = juce::String(gfx::noteFromHz(gfx::hzFromMidi(tuning.strings[size_t(i)])).name);
        auto c = juce::Rectangle<float>(st.getX() + cw * i, st.getY(), cw, st.getHeight())
                     .withSizeKeepingCentre(name.length() > 1 ? 32.0f : 26.0f, 26.0f);
        const bool sel = i == nearest_;
        if (sel) { g.setColour(ink); g.fillRoundedRectangle(c, 13.0f); }
        g.setColour(sel ? bg : dim);
        g.setFont(font(12.5f, sel ? Weight::Bold : Weight::SemiBold));
        g.drawText(name, c, juce::Justification::centred, false);
    }
}

void TunerStrip::resized() {
    auto r = getLocalBounds().reduced(18, 0);
    const int pillH = juce::jmin(36, getHeight() - 16);
    mute_.setBounds(r.removeFromRight(92).withSizeKeepingCentre(92, pillH));
    r.removeFromRight(8);
    reference_.setBounds(r.removeFromRight(86).withSizeKeepingCentre(86, pillH));
    r.removeFromRight(8);
    const int tw = juce::jmax(110, static_cast<int>(textWidth(font(13.5f, Weight::SemiBold), tuning_.getButtonText())) + 36);
    tuning_.setBounds(r.removeFromRight(tw).withSizeKeepingCentre(tw, pillH));
    r.removeFromRight(18);
    stringsArea_ = r.removeFromRight(juce::jmin(220, r.getWidth() / 5)).withSizeKeepingCentre(juce::jmin(220, r.getWidth() / 5), 28);
    r.removeFromRight(20);
    noteArea_ = r.removeFromLeft(58).reduced(0, 8);
    r.removeFromLeft(8);
    infoArea_ = r.removeFromLeft(150).reduced(0, 10);
    r.removeFromLeft(18);
    scaleArea_ = r.withSizeKeepingCentre(r.getWidth(), juce::jmin(32, r.getHeight() - 20));
}

// ---------------------------------------------------------------------------
// AnalysisView
// ---------------------------------------------------------------------------
AnalysisView::AnalysisView(GuitarFXProcessor& proc) : proc_(proc) {
    choose_.onClick = [this] { chooseFile(); };
    create_.setToggleState(true, juce::dontSendNotification);   // primär knapp (svart)
    create_.onClick = [this] {
        if (state_ != State::Done) return;
        proc_.createPresetFromAnalysis(result_, title_);
        if (onClose) onClose();
    };
    close_.onClick = [this] { if (onClose) onClose(); };
    for (auto* c : { &choose_, &create_, &close_ }) addAndMakeVisible(c);
    create_.setVisible(false);
}

AnalysisView::~AnalysisView() { stopWorker(); }

void AnalysisView::stopWorker() {
    cancel_ = true;
    if (worker_.joinable()) worker_.join();
    cancel_ = false;
}

void AnalysisView::setOpen(bool open) {
    open_ = open;
    setVisible(open);
    if (!open) { stopWorker(); stopTimer(); if (state_ == State::Working) state_ = State::Idle; }
    repaint();
}

void AnalysisView::chooseFile() {
    chooser_ = std::make_unique<juce::FileChooser>(S("Välj en låt att analysera"),
        juce::File::getSpecialLocation(juce::File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.m4a;*.ogg");
    juce::Component::SafePointer<AnalysisView> safe(this);
    chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe](const juce::FileChooser& fc) {
                              if (safe && fc.getResult() != juce::File()) safe->start(fc.getResult());
                          });
}

void AnalysisView::start(const juce::File& file) {
    stopWorker();
    title_ = file.getFileNameWithoutExtension();
    state_ = State::Working;
    progress_ = 0.0f;
    finished_ = false;
    error_.clear();
    create_.setVisible(false);
    // Läsning och analys i egen tråd – GUI och ljud påverkas inte.
    worker_ = std::thread([this, file] {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        gfx::SongAnalysis a;
        if (reader == nullptr) {
            error_ = S("Kunde inte läsa filen. Prova WAV, AIFF, FLAC eller MP3.");
        } else {
            const auto maxLen = static_cast<juce::int64>(reader->sampleRate * 360.0);   // högst 6 minuter
            const int n = static_cast<int>(juce::jmin(reader->lengthInSamples, maxLen));
            juce::AudioBuffer<float> buf(juce::jmin(2, static_cast<int>(reader->numChannels)), juce::jmax(1, n));
            reader->read(&buf, 0, n, 0, true, buf.getNumChannels() > 1);
            a = gfx::analyseSong(buf.getReadPointer(0), buf.getNumChannels() > 1 ? buf.getReadPointer(1) : nullptr,
                                 n, reader->sampleRate, [this](float p) { progress_ = p; return !cancel_.load(); });
            if (!a.valid && !cancel_) error_ = S("Filen är för kort eller tyst för att analysera.");
        }
        result_ = a;
        finished_ = true;
    });
    startTimerHz(30);
    repaint();
}

void AnalysisView::timerCallback() {
    if (state_ == State::Working && finished_.load()) {
        if (worker_.joinable()) worker_.join();
        state_ = result_.valid ? State::Done : State::Failed;
        create_.setVisible(state_ == State::Done);
        choose_.setButtonText(state_ == State::Done ? S("Välj annan låt") : S("Välj låt…"));
        stopTimer();
        resized();
    }
    repaint();
}

void AnalysisView::paint(juce::Graphics& g) {
    const auto& p = palette();
    g.fillAll(p.background);
    auto head = getLocalBounds().removeFromTop(60);
    g.setColour(p.ink);
    g.setFont(font(30.0f, Weight::Bold));
    g.drawText(S("Låtanalys"), head, juce::Justification::centredLeft, false);

    auto body = body_;
    if (state_ == State::Idle || state_ == State::Failed) {
        auto box = body.withSizeKeepingCentre(juce::jmin(body.getWidth(), 640), 220);
        g.setColour(p.ink);
        g.setFont(font(26.0f, Weight::SemiBold));
        g.drawText(S("Låt gitarren låta som i en låt"), box.removeFromTop(40), juce::Justification::centred, false);
        g.setColour(state_ == State::Failed ? p.danger : p.secondaryInk);
        g.setFont(font(16.0f));
        const auto text = state_ == State::Failed ? error_
            : S("Välj en ljudfil från datorn. Appen lyssnar på klang, rum, bredd, dynamik och tempo "
                "och föreslår ett preset som du sedan kan justera. Allt sker lokalt.");
        g.drawFittedText(text, box.removeFromTop(90).reduced(20, 0), juce::Justification::centred, 3);
        return;
    }

    if (state_ == State::Working) {
        // Ring av streck som fylls med förloppet (samma formspråk som scenvyn).
        const float d = juce::jmin(static_cast<float>(body.getHeight()) * 0.7f, 360.0f);
        auto ring = body.toFloat().withSizeKeepingCentre(d, d).translated(0, -20);
        const auto c = ring.getCentre();
        constexpr int kTicks = 72;
        const int lit = juce::roundToInt(progress_.load() * kTicks);
        for (int i = 0; i < kTicks; ++i) {
            const float ang = juce::MathConstants<float>::twoPi * static_cast<float>(i) / kTicks;
            const bool on = i < lit;
            const float r1 = d * 0.5f, r0 = r1 - (on ? d * 0.08f : d * 0.05f);
            g.setColour(on ? p.ink : p.line);
            g.drawLine(c.x + r0 * std::sin(ang), c.y - r0 * std::cos(ang), c.x + r1 * std::sin(ang), c.y - r1 * std::cos(ang), on ? 3.0f : 1.4f);
        }
        g.setColour(p.ink);
        g.setFont(font(d * 0.22f, Weight::Light));
        g.drawText(juce::String(juce::roundToInt(progress_.load() * 100.0f)), ring, juce::Justification::centred, false);
        g.setColour(p.secondaryInk);
        g.setFont(font(16.0f));
        g.drawText(S("Analyserar ") + title_ + S("…"), ring.withY(ring.getBottom() + 18.0f).withHeight(24.0f).toNearestInt(),
                   juce::Justification::centred, true);
        return;
    }

    // Resultat: titel och tempo, sedan fyra rutor i rutnätet.
    const auto& a = result_;
    auto top = body.removeFromTop(110);
    g.setColour(p.ink);
    g.setFont(font(34.0f, Weight::Bold));
    g.drawText(title_, top.removeFromTop(48), juce::Justification::bottomLeft, true);
    g.setColour(p.secondaryInk);
    g.setFont(font(16.0f));
    g.drawText((a.bpm > 0 ? juce::String(juce::roundToInt(a.bpm)) + S(" BPM  ·  ") : juce::String())
               + juce::String(juce::roundToInt(a.seconds / 60.0f)) + S(" min  ·  Förslag på preset – justera med öronen"),
               top, juce::Justification::topLeft, true);

    const int gap = kGridGap;
    const int w = (body.getWidth() - gap * (kGridColumns - 1)) / kGridColumns;
    const int h = juce::jmin(body.getHeight() - 70, 300);
    struct Tile { const char* caption; juce::String value; juce::String unit; std::string text; };
    const Tile tiles[4] = {
        { "Klang", minusSign((a.lowGainDb >= 0 ? "+" : "") + juce::String(a.lowGainDb, 1).replaceCharacter('.', ',')), S("dB bas"), a.toneText },
        { "Rum", juce::String(a.reverbDecaySec, 1).replaceCharacter('.', ','), S("s efterklang"), a.roomText },
        { "Bredd", juce::String(juce::roundToInt(a.width * 100.0f)), S("%"), a.widthText },
        { "Dynamik", juce::String(juce::roundToInt(a.loudnessRangeDb)), S("dB spridning"), a.dynamicsText },
    };
    for (int i = 0; i < 4; ++i) {
        auto r = juce::Rectangle<int>(body.getX() + i * (w + gap), body.getY(), w, h);
        drawTile(g, r.toFloat(), 18.0f, p.tile);
        auto inner = r.reduced(20, 18);
        drawCaption(g, S(tiles[i].caption), inner.removeFromTop(18), p.secondaryInk);
        inner.removeFromTop(10);
        auto valueRow = inner.removeFromTop(64);
        g.setColour(p.ink);
        const auto vf = font(54.0f, Weight::Light);
        g.setFont(vf);
        g.drawText(tiles[i].value, valueRow, juce::Justification::bottomLeft, false);
        g.setColour(p.secondaryInk);
        g.setFont(font(14.0f));
        g.drawText(tiles[i].unit, valueRow.withTrimmedLeft(static_cast<int>(textWidth(vf, tiles[i].value)) + 8).withTrimmedBottom(10),
                   juce::Justification::bottomLeft, true);
        inner.removeFromTop(14);
        g.setColour(p.ink);
        g.setFont(font(14.5f));
        g.drawFittedText(u8(tiles[i].text), inner, juce::Justification::topLeft, 6);
    }
}

void AnalysisView::resized() {
    auto r = getLocalBounds();
    auto head = r.removeFromTop(60);
    close_.setBounds(head.removeFromRight(110).withSizeKeepingCentre(110, 48));
    head.removeFromRight(10);
    if (state_ == State::Done) {
        choose_.setBounds(head.removeFromRight(170).withSizeKeepingCentre(170, 48));
    }
    r.removeFromTop(24);
    body_ = r;
    if (state_ == State::Done) {
        auto row = r.removeFromBottom(56);
        create_.setBounds(row.removeFromLeft(200));
    } else if (state_ != State::Working) {
        choose_.setBounds(r.withSizeKeepingCentre(220, 56).translated(0, 120));
    }
    choose_.setVisible(state_ != State::Working);
}

// ---------------------------------------------------------------------------
// HeaderBar
// ---------------------------------------------------------------------------
HeaderBar::HeaderBar() {
    addAndMakeVisible(more);
}

void HeaderBar::paint(juce::Graphics& g) {
    const auto& p = palette();
    g.setColour(p.ink);
    g.setFont(font(40.0f, Weight::Bold).withExtraKerningFactor(-0.02f));
    g.drawText("Stagebox", getLocalBounds(), juce::Justification::centredLeft, false);
}

void HeaderBar::resized() {
    more.setBounds(getLocalBounds().removeFromRight(48).withSizeKeepingCentre(48, 48));
}

} // namespace ui
