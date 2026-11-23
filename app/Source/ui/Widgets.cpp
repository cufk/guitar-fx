#include "Widgets.h"
#include "BinaryData.h"
#include <algorithm>

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

void drawSectionHeading(juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, const juce::String& detail) {
    const auto& p = palette();
    const auto tf = font(22.0f, Weight::SemiBold);
    g.setColour(p.ink);
    g.setFont(tf);
    g.drawText(title, r, juce::Justification::centredLeft, false);
    if (detail.isEmpty()) return;
    // Samma baslinje som rubriken (mindre text ligger annars för högt).
    g.setColour(p.secondaryInk);
    g.setFont(font(13.0f));
    g.drawText(detail, r.withTrimmedLeft(static_cast<int>(textWidth(tf, title)) + 10).translated(0, 3),
               juce::Justification::centredLeft, true);
}

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

float Pill::naturalTextWidth() const {
    return textWidth(font(getHeight() >= 44 ? 15.0f : 13.5f, Weight::SemiBold), getButtonText());
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
    const float baseSize = (r.getHeight() >= 44.0f ? 15.0f : 13.5f) * fontScale;
    auto f = font(baseSize, Weight::SemiBold);
    const juce::String label = getButtonText();
    float w = textWidth(f, label);
    // Smala knappar (t.ex. val i en smal reglageruta): krymp texten upp till 25 % i stället för att korta av.
    if (!showSymbol && prefix.isEmpty() && suffix.isEmpty() && w > r.getWidth() - 28.0f) {
        f = font(baseSize * juce::jlimit(0.72f, 1.0f, (r.getWidth() - 12.0f) / w), Weight::SemiBold);
        w = textWidth(f, label);
        if (w > r.getWidth() - 12.0f) {   // fortfarande för bred: centrera och låt drawText korta av
            g.setColour(!isEnabled() ? p.tertiaryInk : on ? p.inkInverse : p.ink);
            g.setFont(f);
            g.drawText(label, r.reduced(6.0f, 0.0f), juce::Justification::centred, true);
            return;
        }
        g.setColour(!isEnabled() ? p.tertiaryInk : on ? p.inkInverse : p.ink);
        g.setFont(f);
        g.drawText(label, r, juce::Justification::centred, false);
        return;
    }
    const auto pf = font(f.getHeight(), Weight::Regular);
    const float pw = prefix.isNotEmpty() ? textWidth(pf, prefix) + 10.0f : 0.0f;
    const float sw = showSymbol ? 22.0f : 0.0f;
    const auto sf = font(f.getHeight(), Weight::Regular);
    const float xw = suffix.isNotEmpty() ? textWidth(sf, suffix) + 8.0f : 0.0f;
    const float total = juce::jmin(r.getWidth() - 28.0f, w + pw + sw + xw);
    float x = r.getCentreX() - total * 0.5f;
    if (showSymbol) {
        icons::draw(g, symbol, { x, r.getCentreY() - 7.0f, 14.0f, 14.0f }, ink.withMultipliedAlpha(symbolAlpha), 1.7f);
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
    if (suffix.isNotEmpty()) {
        g.setColour(on ? ink.withAlpha(0.7f) : p.secondaryInk);
        g.setFont(sf);
        g.drawText(suffix, juce::Rectangle<float>(x + w + 8.0f, r.getY(), xw, r.getHeight()), juce::Justification::centredLeft, false);
    }
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
    auto r = getLocalBounds().toFloat().withSizeKeepingCentre(52.0f, 30.0f);
    g.setColour((over ? p.tileStrong : p.tile).interpolatedWith(p.ink, pos_));
    g.fillRoundedRectangle(r, 15.0f);
    const float kd = 22.0f;
    const float kx = r.getX() + 4.0f + pos_ * (r.getWidth() - kd - 8.0f);
    g.setColour(p.inkInverse.interpolatedWith(p.background, 0.0f));
    g.setColour(pos_ > 0.5f ? p.inkInverse : p.background);
    g.fillEllipse(kx, r.getCentreY() - kd * 0.5f, kd, kd);
}

// ---------------------------------------------------------------------------
// KnobTile
// ---------------------------------------------------------------------------
KnobTile::KnobTile(const gfx::ParamSpec& spec, bool vertical) : spec_(spec), vertical_(vertical) {
    if (!spec_.choices.empty()) {
        for (size_t i = 0; i < spec_.choices.size(); ++i) {
            auto c = std::make_unique<Pill>(u8(spec_.choices[i]));
            c->setRadioGroupId(1);
            c->onClick = [this, i] { spec_.param->set(static_cast<float>(i)); };
            addAndMakeVisible(*c);
            choices_.push_back(std::move(c));
        }
    } else {
        if (vertical_) {
            knob_.setSliderStyle(juce::Slider::LinearVertical);
            knob_.setSliderSnapsToMousePosition(false);   // dra greppet relativt – inga hopp live
        } else {
            knob_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            knob_.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        }
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
    if (vertical_) {
        drawTile(g, getLocalBounds().toFloat(), 20.0f, p.tile);
        auto bottom = getLocalBounds().reduced(8, 16).removeFromBottom(36);
        g.setColour(p.ink);
        g.setFont(font(13.5f, Weight::Regular));
        g.drawFittedText(u8(spec_.label), bottom.removeFromTop(19), juce::Justification::centred, 1, 0.78f);
        if (choices_.empty()) {
            const float v = spec_.param->get();
            const bool changed = std::abs(v - spec_.defaultValue) > spec_.step * 0.5f;
            g.setColour(changed || knob_.isMouseButtonDown() ? p.ink : p.secondaryInk);
            g.setFont(font(12.5f, Weight::Regular));
            g.drawText(minusSign(formatValue(spec_, v)), bottom, juce::Justification::centred, true);
        }
        return;
    }
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
    if (vertical_) {
        auto r = getLocalBounds().reduced(8, 0).withTrimmedTop(20).withTrimmedBottom(16);
        r.removeFromBottom(36 + 16);   // namn + värde och luft ovanför
        if (!choices_.empty()) {
            const int n = static_cast<int>(choices_.size());
            const int h = juce::jmin(34, (r.getHeight() - 8 * (n - 1)) / n);
            auto col = r.withSizeKeepingCentre(r.getWidth(), n * (h + 8) - 8);
            for (auto& c : choices_) { c->setBounds(col.removeFromTop(h)); col.removeFromTop(8); }
            // Samma textstorlek för alla val: krymp efter det längsta (högst 28 %).
            float widest = 0.0f;
            for (auto& c : choices_) widest = juce::jmax(widest, c->naturalTextWidth());
            const float scale = widest > 0.0f ? juce::jlimit(0.72f, 1.0f, static_cast<float>(r.getWidth() - 28) / widest) : 1.0f;
            for (auto& c : choices_) if (!gfx::sameValue(c->fontScale, scale)) { c->fontScale = scale; c->repaint(); }
        } else {
            knob_.setBounds(r);
        }
        return;
    }
    auto r = getLocalBounds().reduced(16, 14);
    r.removeFromBottom(30);
    if (!choices_.empty()) {
        const int h = juce::jmin(38, (r.getHeight() - 8 * (static_cast<int>(choices_.size()) - 1)) / static_cast<int>(choices_.size()));
        auto col = r.withSizeKeepingCentre(r.getWidth(), static_cast<int>(choices_.size()) * (h + 8) - 8);
        for (auto& c : choices_) { c->setBounds(col.removeFromTop(h)); col.removeFromTop(8); }
    } else {
        // Ratten lite mindre än rutan – luft runt omkring.
        const int d = juce::roundToInt(static_cast<float>(juce::jmin(r.getWidth(), r.getHeight())) * 0.8f);
        knob_.setBounds(r.withSizeKeepingCentre(d, d));
    }
}

// ---------------------------------------------------------------------------
// EffectGrid
// ---------------------------------------------------------------------------
EffectGrid::EffectGrid(GuitarFXProcessor& proc, std::function<void(int)> onSelect)
    : proc_(proc), onSelect_(std::move(onSelect)) {
    const auto& effects = proc_.rig().effects();
    for (int i = 0; i < static_cast<int>(effects.size()); ++i) order_.push_back(i);
    std::stable_sort(order_.begin(), order_.end(), [&](int a, int b) { return effects[size_t(a)].slot < effects[size_t(b)].slot; });
}

juce::Rectangle<int> EffectGrid::cell(int pos) const {
    const int cols = kControlColumns, gap = kGridGap;   // samma sju kolumner som reglagen
    const int top = size::sectionHead + size::sectionGap;
    const int w = (getWidth() - gap * (cols - 1)) / cols;
    return { (pos % cols) * (w + gap), top + (pos / cols) * (kTileH + gap), w, kTileH };
}

int EffectGrid::cellAt(juce::Point<int> pt) const {
    for (int pos = 0; pos < static_cast<int>(order_.size()); ++pos) if (cell(pos).contains(pt)) return order_[size_t(pos)];
    return -1;
}

void EffectGrid::paint(juce::Graphics& g) {
    const auto& p = palette();
    const auto& effects = proc_.rig().effects();
    drawSectionHeading(g, getLocalBounds().removeFromTop(size::sectionHead), "Effekter", S("i signalordning"));
    for (int pos = 0; pos < static_cast<int>(order_.size()); ++pos) {
        const int i = order_[size_t(pos)];
        const auto& e = effects[size_t(i)];
        const bool on = proc_.engine().chain().isEnabled(e.slot);
        const bool hover = hover_ == i;
        auto r = cell(pos).toFloat();
        // Svart = på (samma betydelse som aktiv rad i setlistan).
        drawTile(g, r, 16.0f, on ? p.ink.interpolatedWith(p.background, hover ? 0.12f : 0.0f) : (hover ? p.tileStrong : p.tile));
        const auto ink = on ? p.inkInverse : p.ink;
        auto inner = r.reduced(12.0f, 0.0f).withTrimmedTop(12.0f).withTrimmedBottom(10.0f);
        icons::drawEffect(g, e.id, inner.removeFromTop(20.0f).removeFromLeft(20.0f), ink);
        auto dot = juce::Rectangle<float>(r.getRight() - 21.0f, r.getY() + 17.5f, 9.0f, 9.0f);
        if (on) { g.setColour(ink); g.fillEllipse(dot); }
        else { g.setColour(p.secondaryInk); g.drawEllipse(dot.reduced(0.75f), 1.5f); }
        // Namn och under det en rad för märkning (I PRESET / antal filter).
        auto tag = inner.removeFromBottom(12.0f);
        g.setColour(ink);
        g.setFont(font(13.5f, Weight::SemiBold));
        g.drawFittedText(u8(e.name), inner.removeFromBottom(18.0f).toNearestInt(), juce::Justification::bottomLeft, 1, 0.8f);
        juce::String mark;
        if (e.id == "feedback" && on && e.activity) {
            const int n = juce::roundToInt(e.activity());
            if (n > 0) mark = juce::String(n) + " FILTER";
        }
        if (mark.isEmpty() && proc_.rig().activeIncludes(e.id)) mark = S("I PRESET");
        if (mark.isNotEmpty()) {
            g.setColour(on ? p.inkInverse.withAlpha(0.65f) : p.secondaryInk);
            g.setFont(font(9.5f, Weight::SemiBold).withExtraKerningFactor(0.08f));
            g.drawText(mark, tag.toNearestInt(), juce::Justification::centredLeft, false);
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
EffectPanel::EffectPanel(GuitarFXProcessor& proc) : proc_(proc), filters_(proc) {
    switch_.onClick = [this] {
        if (index_ >= 0) proc_.engine().chain().setEnabled(proc_.rig().effects()[size_t(index_)].slot, switch_.getToggleState());
    };
    presetPill_.setTooltip(S("Ingår i det aktiva presetet. Stäng av för att effekten ska följa grundljudet igen."));
    presetPill_.symbol = icons::Symbol::Check;
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
    tap_.symbol = icons::Symbol::Dot;
    tap_.showSymbol = true;
    tap_.setTooltip(S("Tryck i takt för delayns tempo (mellanslag / fotswitch CC 107)"));
    tap_.onClick = [this] { proc_.tapTempo(); };
    resetFilters_.setTooltip(S("Ta bort alla filter, t.ex. efter soundcheck eller på en ny scen"));
    resetFilters_.onClick = [this] { proc_.feedbackSuppressor().requestReset(); };
    addChildComponent(tap_);
    addChildComponent(resetFilters_);
    addChildComponent(filters_);
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
    const auto& id = effects[size_t(i)].id;
    loadIr_->setVisible(id == "cabinet");
    builtInIr_->setVisible(id == "cabinet");
    tap_.setVisible(id == "delay");
    resetFilters_.setVisible(id == "feedback");
    filters_.setVisible(id == "feedback");
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
    const bool included = presetActive && rig.activeIncludes(e.id);
    presetPill_.setToggleState(included, juce::dontSendNotification);
    if (presetPill_.showSymbol != included) { presetPill_.showSymbol = included; presetPill_.repaint(); }
    activity_ = (on && e.activity) ? e.activity() : 0.0f;
    for (auto& t : tiles_) t->refresh();
    if (tap_.isVisible()) {
        // Tempo som text och en prick som blinkar på varje taktslag (räknat från senaste tap).
        const float bpm = proc_.delay().tempoBpm.get();
        const auto text = juce::String(juce::roundToInt(bpm)) + " BPM";
        if (tap_.suffix != text) { tap_.suffix = text; tap_.repaint(); }
        const double beat = 60000.0 / juce::jmax(1.0f, bpm);
        const double since = juce::Time::getMillisecondCounterHiRes() - proc_.lastTapMs();
        const float alpha = std::fmod(since, beat) < 90.0 ? 1.0f : 0.25f;
        if (!gfx::sameValue(tap_.symbolAlpha, alpha)) { tap_.symbolAlpha = alpha; tap_.repaint(); }
    }
    if (filters_.isVisible()) filters_.repaint();
    repaint(0, 0, getWidth(), kHead);
}

void EffectPanel::paint(juce::Graphics& g) {
    if (index_ < 0) return;
    const auto& p = palette();
    const auto& e = proc_.rig().effects()[size_t(index_)];
    auto& rig = proc_.rig();
    const bool on = switch_.getToggleState();

    // Tunn linje mellan effektrutorna och den valda effekten.
    g.setColour(p.line);
    g.fillRect(0, 12, getWidth(), 1);

    int right = switch_.getX();
    if (presetPill_.isVisible()) right = presetPill_.getX();
    if (loadIr_->isVisible()) right = loadIr_->getX();
    if (tap_.isVisible()) right = tap_.getX();
    if (resetFilters_.isVisible()) right = resetFilters_.getX();
    auto head = juce::Rectangle<int>(0, kHead - size::sectionHead, juce::jmax(0, right - 24), size::sectionHead);

    const juce::String title = u8(e.title.empty() ? e.name : e.title);
    juce::String status;
    if (rig.activePreset() < 0) status = on ? S("På i grundljudet") : S("Av");
    else if (rig.activeIncludes(e.id)) status = (on ? S("På") : S("Av")) + S(" · sparas i presetet ”") + u8(rig.activeName()) + S("”");
    else status = on ? S("På · följer grundljudet – ändra för att spara i presetet") : S("Av · följer grundljudet – slå på eller ändra för att spara i presetet");
    if (e.id == "cabinet") status << S(" · ") << u8(proc_.cabinet().impulseResponseName());
    if (e.id == "sustain" && activity_ < -0.5f) status << S(" · ") << juce::String(juce::roundToInt(-activity_)) << S(" dB kompression");
    if (e.id == "feedback" && on) {
        const int n = juce::roundToInt(activity_);
        status << S(" · ") << (n == 0 ? S("inga filter än") : juce::String(n) + (n == 1 ? S(" filter aktivt") : S(" filter aktiva")));
    }
    if (irStatus_.isNotEmpty()) {
        drawSectionHeading(g, head, title, {});
        g.setColour(p.danger);
        g.setFont(font(13.0f));
        g.drawText(irStatus_, head.withTrimmedLeft(static_cast<int>(textWidth(font(22.0f, Weight::SemiBold), title)) + 10).translated(0, 3),
                   juce::Justification::centredLeft, true);
    } else {
        drawSectionHeading(g, head, title, status);
    }
}

void EffectPanel::resized() {
    auto r = getLocalBounds();
    auto head = r.removeFromTop(kHead).withTrimmedTop(kHead - size::sectionHead);
    const int sb = size::smallButton;
    switch_.setBounds(head.removeFromRight(56).withSizeKeepingCentre(56, 34));
    head.removeFromRight(12);
    if (presetPill_.isVisible()) {
        presetPill_.setBounds(head.removeFromRight(124).withSizeKeepingCentre(124, sb));
        head.removeFromRight(8);
    }
    if (loadIr_->isVisible()) {
        builtInIr_->setBounds(head.removeFromRight(96).withSizeKeepingCentre(96, sb));
        head.removeFromRight(8);
        loadIr_->setBounds(head.removeFromRight(110).withSizeKeepingCentre(110, sb));
    }
    if (tap_.isVisible()) tap_.setBounds(head.removeFromRight(130).withSizeKeepingCentre(130, sb));
    if (resetFilters_.isVisible()) resetFilters_.setBounds(head.removeFromRight(104).withSizeKeepingCentre(104, sb));
    r.removeFromTop(size::sectionGap);

    const int n = static_cast<int>(tiles_.size());
    if (n == 0) return;
    // Lodräta reglage som fyller höjden, i sju lika breda platser (flest reglage = EQ:s sju):
    // ett reglage har alltid samma storlek oavsett effekt.
    constexpr int cols = kControlColumns;
    const int gap = kGridGap;
    const int rows = (n + cols - 1) / cols;
    const int w = (r.getWidth() - gap * (cols - 1)) / cols;
    const int h = (r.getHeight() - gap * (rows - 1)) / rows;
    for (int i = 0; i < n; ++i)
        tiles_[size_t(i)]->setBounds(r.getX() + (i % cols) * (w + gap), r.getY() + (i / cols) * (h + gap), w, h);
    // Rundgångsskydd: filtervyn tar de lediga platserna i raden.
    if (filters_.isVisible() && n < cols)
        filters_.setBounds(r.getX() + n * (w + gap), r.getY(), r.getRight() - (r.getX() + n * (w + gap)), h);
}

void FilterView::paint(juce::Graphics& g) {
    const auto& p = palette();
    auto& fb = proc_.feedbackSuppressor();
    drawTile(g, getLocalBounds().toFloat(), 20.0f, p.tile);
    auto r = getLocalBounds().reduced(20, 0).withTrimmedTop(20).withTrimmedBottom(16);
    auto head = r.removeFromTop(18);
    drawCaption(g, S("Aktiva filter"), head, p.secondaryInk);
    g.setColour(p.secondaryInk);
    g.setFont(font(12.0f));
    g.drawText(S("djup per filter"), head, juce::Justification::centredRight, false);
    r.removeFromTop(16);

    // Använda filter sorterade efter frekvens, sedan lediga platser upp till "Max filter".
    std::vector<std::pair<float, float>> used;
    for (int i = 0; i < gfx::FeedbackSuppressor::kMaxFilters; ++i)
        if (fb.filterHz(i) > 0.0f) used.push_back({ fb.filterHz(i), fb.filterDepthDb(i) });
    std::sort(used.begin(), used.end());
    const int slots = juce::jlimit(1, gfx::FeedbackSuppressor::kMaxFilters,
                                   juce::jmax(static_cast<int>(used.size()), juce::roundToInt(fb.maxFilters.get())));
    const int gap = 10;
    const int cw = (r.getWidth() - gap * (slots - 1)) / slots;
    for (int i = 0; i < slots; ++i) {
        auto col = juce::Rectangle<int>(r.getX() + i * (cw + gap), r.getY(), cw, r.getHeight());
        auto labels = col.removeFromBottom(34);
        col.removeFromBottom(10);
        g.setColour(p.tileStrong);
        g.fillRoundedRectangle(col.toFloat(), 10.0f);
        juce::String hz = S("–"), db = S("ledig");
        if (i < static_cast<int>(used.size())) {
            const auto [f, d] = used[size_t(i)];
            const float depth = juce::jlimit(0.0f, 1.0f, -d / 30.0f);
            g.setColour(p.ink);
            g.fillRoundedRectangle(col.toFloat().withHeight(juce::jmax(20.0f, static_cast<float>(col.getHeight()) * depth)), 10.0f);
            hz = (f >= 1000.0f ? juce::String(f / 1000.0f, 2).replaceCharacter('.', ',') + " kHz" : juce::String(juce::roundToInt(f)) + " Hz");
            db = minusSign(juce::String(juce::roundToInt(d))) + " dB";
        }
        g.setColour(p.ink);
        g.setFont(font(12.5f, Weight::SemiBold));
        g.drawFittedText(hz, labels.removeFromTop(17), juce::Justification::centred, 1, 0.75f);
        g.setColour(p.secondaryInk);
        g.setFont(font(11.5f));
        g.drawText(db, labels, juce::Justification::centredTop, false);
    }
}

// ---------------------------------------------------------------------------
// SetlistPanel
// ---------------------------------------------------------------------------
SetlistPanel::SetlistPanel(GuitarFXProcessor& proc) : proc_(proc) {
    add_.onClick = [this] {
        auto& rig = proc_.rig();
        const int i = rig.addPreset("Preset " + std::to_string(rig.presets().size() + 1));
        rig.activate(i);
        rename(i);
    };
    prev_.onClick = [this] { proc_.rig().selectPrevious(); };
    next_.onClick = [this] { proc_.rig().selectNext(); };
    prev_.setTooltip(S("Föregående i setlistan (← / fotswitch CC 102)"));
    next_.setTooltip(S("Nästa i setlistan (→ / fotswitch CC 103)"));
    for (juce::Component* c : std::initializer_list<juce::Component*> { &add_, &analyse, &prev_, &next_ }) addAndMakeVisible(c);
    refresh();
}

void SetlistPanel::refresh() {
    auto& rig = proc_.rig();
    if (rig.activePreset() != lastActive_) { lastActive_ = rig.activePreset(); scrollToActive(); }
    if (rig.revision() != lastRevision_) { lastRevision_ = rig.revision(); scroll_ = juce::jlimit(0, maxScroll(), scroll_); repaint(); }
}

void SetlistPanel::timerCallback() {}

juce::Rectangle<int> SetlistPanel::rowBounds(int row) const {
    return { list_.getX(), list_.getY() + row * (kRowH + kRowGap), list_.getWidth(), kRowH };
}

int SetlistPanel::maxScroll() const {
    const int n = static_cast<int>(proc_.rig().presets().size()) + 1;
    return juce::jmax(0, n * (kRowH + kRowGap) - kRowGap - list_.getHeight());
}

int SetlistPanel::rowAt(int y) const {
    if (y < list_.getY() || y > list_.getBottom()) return -2;
    const int row = (y - list_.getY() + scroll_) / (kRowH + kRowGap);
    const int n = static_cast<int>(proc_.rig().presets().size()) + 1;
    return row >= 0 && row < n ? row - 1 : -2;
}

void SetlistPanel::scrollToActive() {
    const auto r = rowBounds(proc_.rig().activePreset() + 1);
    const int top = r.getY() - list_.getY(), bottom = r.getBottom() - list_.getY();
    if (top < scroll_) scroll_ = top;
    else if (bottom > scroll_ + list_.getHeight()) scroll_ = bottom - list_.getHeight();
    scroll_ = juce::jlimit(0, maxScroll(), scroll_);
    repaint();
}

void SetlistPanel::paint(juce::Graphics& g) {
    const auto& p = palette();
    auto& rig = proc_.rig();
    const int count = static_cast<int>(rig.presets().size());
    juce::String detail;
    if (rig.activePreset() >= 0) detail = juce::String(rig.activePreset() + 1) + " av " + juce::String(count);
    else detail = count == 0 ? S("Inga presets än") : juce::String(count) + (count == 1 ? " preset" : " presets");
    drawSectionHeading(g, head_.withTrimmedRight(head_.getRight() - analyse.getX()), "Setlist", detail);

    g.saveState();
    g.reduceClipRegion(list_);
    const int n = static_cast<int>(rig.presets().size()) + 1;
    auto drawRow = [&](int row, juce::Rectangle<int> r, bool floating) {
        const int presetIndex = row - 1;
        const bool active = rig.activePreset() == presetIndex;
        auto bg = active ? p.ink : (hover_ == presetIndex && !dragging_ ? p.tileStrong : p.tile);
        if (floating) bg = bg.withAlpha(0.95f);
        g.setColour(bg);
        g.fillRoundedRectangle(r.toFloat(), 16.0f);
        const auto ink = active ? p.inkInverse : p.ink;
        const auto sub = active ? p.inkInverse.withAlpha(0.65f) : p.secondaryInk;
        auto inner = r.reduced(16, 0);
        // Nummer (= tangent / fotswitch Program Change + 1).
        g.setColour(sub);
        g.setFont(font(13.5f, Weight::Regular));
        g.drawText(juce::String(row), inner.removeFromLeft(26), juce::Justification::centredLeft, false);
        // Draghandtag (inte för grundljudet).
        if (row > 0) {
            auto h = inner.removeFromRight(22).toFloat().withSizeKeepingCentre(16.0f, 12.0f);
            g.setColour(sub.withAlpha(hover_ == presetIndex || floating ? 1.0f : 0.45f));
            for (int k = 0; k < 3; ++k) g.fillRoundedRectangle(h.getX(), h.getY() + k * 5.0f, h.getWidth(), 2.0f, 1.0f);
        }
        juce::String name, names;
        if (row == 0) {
            name = S("Grundljud");
            juce::StringArray on;
            for (const auto& e : rig.effects()) { auto it = rig.base().find(e.id); if (it != rig.base().end() && it->second.enabled) on.add(u8(e.name)); }
            names = on.isEmpty() ? S("Inga effekter på") : S("Alltid på: ") + on.joinIntoString(S(" · "));
        } else {
            const auto& pr = rig.presets()[size_t(presetIndex)];
            name = u8(pr.name);
            juce::StringArray inc;
            for (const auto& e : rig.effects()) if (pr.overrides.count(e.id)) inc.add(u8(e.name));
            names = inc.isEmpty() ? S("Tomt – ställ in effekter medan presetet är aktivt") : inc.joinIntoString(S(" · "));
        }
        g.setColour(ink);
        g.setFont(font(15.5f, Weight::SemiBold));
        g.drawText(name, inner.withTrimmedBottom(inner.getHeight() / 2 - 1), juce::Justification::bottomLeft, true);
        g.setColour(sub);
        g.setFont(font(12.5f));
        g.drawText(names, inner.withTrimmedTop(inner.getHeight() / 2 + 3), juce::Justification::topLeft, true);
    };
    for (int row = 0; row < n; ++row) {
        if (dragging_ && row - 1 == dragFrom_) continue;   // ritas flytande nedan
        int visualRow = row;
        if (dragging_ && row > 0) {
            // Flytta undan raderna mellan ursprung och mål.
            const int pi = row - 1;
            if (dragFrom_ < dragTo_ && pi > dragFrom_ && pi <= dragTo_) visualRow = row - 1;
            if (dragFrom_ > dragTo_ && pi >= dragTo_ && pi < dragFrom_) visualRow = row + 1;
        }
        drawRow(row, rowBounds(visualRow).translated(0, -scroll_), false);
    }
    if (dragging_ && dragFrom_ >= 0) {
        auto r = rowBounds(dragFrom_ + 1).withY(dragY_ - kRowH / 2);
        juce::Path shape;
        shape.addRoundedRectangle(r.toFloat(), 16.0f);
        juce::DropShadow(juce::Colours::black.withAlpha(0.2f), 16, { 0, 6 }).drawForPath(g, shape);
        drawRow(dragFrom_ + 1, r, true);
    }
    g.restoreState();
}

void SetlistPanel::resized() {
    auto r = getLocalBounds();
    head_ = r.removeFromTop(size::sectionHead);
    auto head = head_;
    const int b = size::button;
    add_.setBounds(head.removeFromRight(b).withSizeKeepingCentre(b, b));
    head.removeFromRight(8);
    analyse.setBounds(head.removeFromRight(b).withSizeKeepingCentre(b, b));
    r.removeFromTop(size::sectionGap);
    // Föregående mot vänsterkanten, Nästa mot högerkanten – lika höga som övriga knappar.
    auto nav = r.removeFromBottom(b);
    const auto pf = font(13.5f, Weight::SemiBold);
    const int pw = static_cast<int>(textWidth(pf, prev_.getButtonText())) + 44;
    const int nw = static_cast<int>(textWidth(pf, next_.getButtonText())) + 44;
    prev_.setBounds(nav.removeFromLeft(pw));
    next_.setBounds(nav.removeFromRight(nw));
    r.removeFromBottom(size::sectionGap);
    list_ = r;
    scroll_ = juce::jlimit(0, maxScroll(), scroll_);
}

void SetlistPanel::mouseDown(const juce::MouseEvent& e) {
    downTime_ = juce::Time::getMillisecondCounter();
    popupDown_ = e.mods.isPopupMenu();
    pressRow_ = rowAt(e.y);
    dragging_ = false;
}

void SetlistPanel::mouseDrag(const juce::MouseEvent& e) {
    if (pressRow_ < 0 || popupDown_) return;   // grundljudet flyttas inte
    if (!dragging_ && e.getDistanceFromDragStartY() != 0 && std::abs(e.getDistanceFromDragStartY()) > 6) {
        dragging_ = true;
        dragFrom_ = dragTo_ = pressRow_;
    }
    if (!dragging_) return;
    dragY_ = juce::jlimit(list_.getY() + kRowH / 2, list_.getBottom() - kRowH / 2, e.y);
    const int n = static_cast<int>(proc_.rig().presets().size());
    const int row = (dragY_ - list_.getY() + scroll_) / (kRowH + kRowGap) - 1;
    dragTo_ = juce::jlimit(0, n - 1, row);
    // Autoscroll nära kanterna.
    if (e.y < list_.getY() + 20) scroll_ = juce::jmax(0, scroll_ - 12);
    if (e.y > list_.getBottom() - 20) scroll_ = juce::jmin(maxScroll(), scroll_ + 12);
    repaint();
}

void SetlistPanel::mouseUp(const juce::MouseEvent& e) {
    if (dragging_) {
        if (dragFrom_ != dragTo_) proc_.rig().movePreset(dragFrom_, dragTo_);
        dragging_ = false;
        dragFrom_ = dragTo_ = -1;
        repaint();
        return;
    }
    const int row = rowAt(e.y);
    if (row == -2 || row != pressRow_) return;
    const bool menu = popupDown_ || juce::Time::getMillisecondCounter() - downTime_ >= 500;
    if (row >= 0 && menu) { showMenu(row); return; }
    proc_.rig().activate(row);
}

void SetlistPanel::mouseMove(const juce::MouseEvent& e) {
    const int row = rowAt(e.y);
    setMouseCursor(row >= 0 ? juce::MouseCursor::DraggingHandCursor
                 : row == -1 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    if (row != hover_) { hover_ = row; repaint(); }
}

void SetlistPanel::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) {
    scroll_ = juce::jlimit(0, maxScroll(), scroll_ - juce::roundToInt(w.deltaY * 240.0f));
    repaint();
}

void SetlistPanel::showMenu(int i) {
    auto& rig = proc_.rig();
    if (i < 0 || i >= static_cast<int>(rig.presets().size())) return;
    const int n = static_cast<int>(rig.presets().size());
    juce::PopupMenu m;
    m.addSectionHeader(u8(rig.presets()[size_t(i)].name));
    m.addItem(1, S("Byt namn…"));
    m.addItem(2, "Duplicera");
    m.addItem(3, S("Flytta upp"), i > 0);
    m.addItem(4, S("Flytta ned"), i < n - 1);
    m.addSeparator();
    m.addItem(5, S("Ta bort…"));
    juce::Component::SafePointer<SetlistPanel> safe(this);
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
                    juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::NoIcon)
                        .withTitle(S("Ta bort ") + u8(model.presets()[size_t(i)].name) + "?")
                        .withMessage(S("Presetet tas bort ur setlistan. Grundljudet och andra presets påverkas inte."))
                        .withButton("Ta bort", 1).withButton("Avbryt", 0).withAssociatedComponent(safe.getComponent()),
                    [safe, i](int res) { if (safe && res == 1) safe->proc_.rig().removePreset(i); });
                break;
            default: break;
        }
    });
}

void SetlistPanel::rename(int i) {
    auto& rig = proc_.rig();
    if (i < 0 || i >= static_cast<int>(rig.presets().size())) return;
    auto* w = new juce::AlertWindow(S("Namn på preset"), S("Ge presetet ett namn du känner igen på scen, t.ex. låtens namn."),
                                    juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor("name", u8(rig.presets()[size_t(i)].name));
    w->addButton("Spara", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Avbryt", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<SetlistPanel> safe(this);
    w->enterModalState(true, juce::ModalCallbackFunction::create([safe, w, i](int r) {
        if (!safe || r != 1) return;
        const auto name = w->getTextEditorContents("name").trim();
        if (name.isNotEmpty()) safe->proc_.rig().renamePreset(i, name.toStdString());
    }), true);
}

// ---------------------------------------------------------------------------
// InputMeter
// ---------------------------------------------------------------------------
void InputMeter::update() {
    auto& e = proc_.engine();
    for (int ch = 0; ch < 2; ++ch) {
        const float v = e.inputPeak(ch);
        level_[ch] = juce::jmax(v, level_[ch] * 0.85f);
        if (v >= hold_[ch]) { hold_[ch] = v; holdTicks_[ch] = 40; }
        else if (--holdTicks_[ch] <= 0) hold_[ch] *= 0.9f;
    }
    repaint();
}

int InputMeter::pillWidth() const {
    const int n = proc_.engine().input().mode.load() == 2 ? 2 : 1;
    return 18 * 2 + n * (30 + 10 + (n > 1 ? 110 : 140)) + (n - 1) * 22;
}

void InputMeter::paint(juce::Graphics& g) {
    const auto& p = palette();
    const int mode = proc_.engine().input().mode.load();
    std::vector<int> chans;
    if (mode == 0 || mode == 2) chans.push_back(0);
    if (mode == 1 || mode == 2) chans.push_back(1);
    const int n = static_cast<int>(chans.size());
    const int labelW = 30, barW = n > 1 ? 110 : 140, segGap = 22, pad = 18;
    const int w = pillWidth();
    auto pill = getLocalBounds().removeFromRight(w).withSizeKeepingCentre(w, size::button);
    drawTile(g, pill.toFloat(), pill.getHeight() * 0.5f, p.tile);
    auto r = pill.reduced(pad, 0);
    for (int ch : chans) {
        auto seg = r.removeFromLeft(labelW + 10 + barW);
        r.removeFromLeft(segGap);
        drawCaption(g, ch == 0 ? S("In 1") : S("In 2"), seg.removeFromLeft(labelW), p.secondaryInk);
        seg.removeFromLeft(10);
        auto bar = seg.withSizeKeepingCentre(seg.getWidth(), 6).toFloat();
        g.setColour(p.tileStrong);
        g.fillRoundedRectangle(bar, 3.0f);
        const float lv = levelNorm(level_[ch]);
        if (lv > 0.0f) {
            g.setColour(lv > 0.985f ? p.danger : p.ink);
            g.fillRoundedRectangle(bar.withWidth(juce::jmax(6.0f, bar.getWidth() * lv)), 3.0f);
        }
        const float h = levelNorm(hold_[ch]);
        if (h > 0.02f) {
            g.setColour(h > 0.985f ? p.danger : p.ink);
            g.fillRoundedRectangle(bar.getX() + bar.getWidth() * h - 1.5f, bar.getY() - 2.0f, 3.0f, bar.getHeight() + 4.0f, 1.5f);
        }
    }
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
      balance_(balanceSpec_, false), align_(alignSpec_, false), inGain_(inSpec_, false), outGain_(outSpec_, false) {
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
        pill->offFill = muted ? p.inkInverse.withAlpha(0.14f) : p.tileStrong;
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
    drawTile(g, getLocalBounds().toFloat(), 20.0f, bg);

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
    auto r = getLocalBounds().withTrimmedLeft(24).withTrimmedRight(14);
    const int pillH = juce::jmin(size::button, getHeight() - 16);
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
    boost.symbol = icons::Symbol::ArrowUp;
    boost.showSymbol = true;
    boost.setClickingTogglesState(false);
    boost.setTooltip(S("Solo-boost på/av (B / fotswitch CC 106). Mängden ställs in i Volym."));
    addAndMakeVisible(boost);
}

void HeaderBar::paint(juce::Graphics& g) {
    // Logotypen (märke + ordmärke) från Resources/logo – mörk variant i mörkt läge.
    const bool dark = isDarkMode();
    if (!logo_ || logoDark_ != dark) {
        logo_ = dark ? juce::Drawable::createFromImageData(BinaryData::stageboxlogodark_svg, BinaryData::stageboxlogodark_svgSize)
                     : juce::Drawable::createFromImageData(BinaryData::stageboxlogolight_svg, BinaryData::stageboxlogolight_svgSize);
        logoDark_ = dark;
    }
    if (logo_) {
        // Placera efter SVG:ns egen ritruta (1200 × 314) – inte efter innehållets gränser, som
        // inte räknar med linjernas tjocklek. I ritrutan börjar märket 5,64 % in från vänster och
        // märkets mitt ligger 46,6 % ned. Höjd 48 px ger versalhöjd ≈ 23 px (som förra rubriken).
        const float h = 48.0f, w = h * 1200.0f / 314.0f;
        const juce::Rectangle<float> dest(-0.0564f * w, static_cast<float>(getHeight()) * 0.5f - 0.466f * h, w, h);
        logo_->draw(g, 1.0f, juce::RectanglePlacement(juce::RectanglePlacement::stretchToFit)
                                 .getTransformToFit({ 0.0f, 0.0f, 1200.0f, 314.0f }, dest));
    } else {
        g.setColour(palette().ink);
        g.setFont(font(30.0f, Weight::Bold).withExtraKerningFactor(-0.02f));
        g.drawText("Stagebox", getLocalBounds(), juce::Justification::centredLeft, false);
    }
}

void HeaderBar::resized() {
    more.setBounds(getLocalBounds().removeFromRight(size::button).withSizeKeepingCentre(size::button, size::button));
}

} // namespace ui
