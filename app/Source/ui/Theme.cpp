#include "Theme.h"
#include <cmath>

namespace ui {

namespace {
Palette makePalette(bool dark) {
    Palette p;
    auto w = [](float v) { return juce::Colour::fromFloatRGBA(v, v, v, 1.0f); };
    p.background   = dark ? w(0.11f)  : w(0.925f);
    p.card         = dark ? w(0.17f)  : juce::Colours::white;
    p.subtle       = dark ? w(0.22f)  : w(0.955f);
    p.fill         = dark ? w(0.30f)  : w(0.885f);
    p.ink          = dark ? w(0.95f)  : w(0.08f);
    p.inkInverse   = dark ? w(0.08f)  : juce::Colours::white;
    p.secondaryInk = dark ? w(0.62f)  : w(0.45f);
    return p;
}
bool gDark = false;
Palette gPalette = makePalette(false);
} // namespace

const Palette& palette() { return gPalette; }
void setDarkMode(bool dark) { gDark = dark; gPalette = makePalette(dark); }
bool isDarkMode() { return gDark; }

void drawCard(juce::Graphics& g, juce::Rectangle<float> r, float radius, juce::Colour fill) {
    juce::Path p;
    p.addRoundedRectangle(r, radius);
    juce::DropShadow(juce::Colours::black.withAlpha(gDark ? 0.25f : 0.07f), 14, { 0, 2 }).drawForPath(g, p);
    g.setColour(fill);
    g.fillPath(p);
}

juce::Font font(float size, bool bold) {
    return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
}

juce::Font captionFont() {
    return juce::Font(juce::FontOptions(10.5f, juce::Font::bold)).withExtraKerningFactor(0.06f);
}

juce::String formatValue(const gfx::ParamSpec& spec, float v) {
    if (!spec.choices.empty()) {
        const int i = juce::jlimit(0, static_cast<int>(spec.choices.size()) - 1, juce::roundToInt(v));
        return u8(spec.choices[static_cast<size_t>(i)]);
    }
    juce::String s;
    if (spec.unit.empty() && spec.param->max() <= 1.0f) {
        s = juce::String(juce::roundToInt(v * 100.0f)) + " %";
    } else if (spec.unit == "Hz" && v >= 1000.0f) {
        s = juce::String(v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz";
    } else {
        s = spec.decimals == 0 ? juce::String(juce::roundToInt(v)) : juce::String(v, spec.decimals);
        if (!spec.unit.empty()) s << " " << u8(spec.unit);
    }
    return s.replaceCharacter('.', ',');  // svensk decimalkomma
}

// ---------------------------------------------------------------------------
LookAndFeel::LookAndFeel() { refreshColours(); }

void LookAndFeel::refreshColours() {
    const auto& p = palette();
    setColour(juce::ResizableWindow::backgroundColourId, p.background);
    setColour(juce::Label::textColourId, p.ink);
    setColour(juce::TextButton::buttonColourId, p.subtle);
    setColour(juce::TextButton::buttonOnColourId, p.ink);
    setColour(juce::TextButton::textColourOffId, p.ink);
    setColour(juce::TextButton::textColourOnId, p.inkInverse);
    setColour(juce::PopupMenu::backgroundColourId, p.card);
    setColour(juce::PopupMenu::textColourId, p.ink);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, p.accent);
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour(juce::AlertWindow::backgroundColourId, p.card);
    setColour(juce::AlertWindow::textColourId, p.ink);
    setColour(juce::AlertWindow::outlineColourId, p.fill);
    setColour(juce::TextEditor::backgroundColourId, p.subtle);
    setColour(juce::TextEditor::textColourId, p.ink);
    setColour(juce::TextEditor::outlineColourId, p.fill);
    setColour(juce::TextEditor::focusedOutlineColourId, p.accent);
    setColour(juce::ScrollBar::thumbColourId, p.fill);
    setColour(juce::Slider::thumbColourId, p.accent);
    setColour(juce::CaretComponent::caretColourId, p.accent);
}

// Smalt reglage: tunn bana, fylld del i reglagets färg och en liten knopp (MiniSlider).
void LookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                   juce::Slider::SliderStyle, juce::Slider& s) {
    const auto& p = palette();
    const auto tint = s.isEnabled() ? s.findColour(juce::Slider::thumbColourId) : p.secondaryInk;
    const float cy = static_cast<float>(y) + static_cast<float>(h) * 0.5f;
    const float left = static_cast<float>(x) + 7.0f, right = static_cast<float>(x + w) - 7.0f;
    const float px = juce::jlimit(left, right, pos);

    g.setColour(p.fill);
    g.fillRoundedRectangle(left, cy - 2.0f, right - left, 4.0f, 2.0f);

    // Bipolära reglage (t.ex. ±12 dB) fylls från mitten.
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float zeroX = bipolar ? left + (right - left) * static_cast<float>(s.valueToProportionOfLength(0.0)) : left;
    g.setColour(tint);
    g.fillRoundedRectangle(juce::jmin(zeroX, px), cy - 2.0f, juce::jmax(4.0f, std::abs(px - zeroX)), 4.0f, 2.0f);

    const float r = 7.0f;
    juce::Path knob;
    knob.addEllipse(px - r, cy - r, 2 * r, 2 * r);
    juce::DropShadow(juce::Colours::black.withAlpha(0.12f), 3, { 0, 1 }).drawForPath(g, knob);
    g.setColour(p.card);
    g.fillPath(knob);
    g.setColour(tint);
    g.strokePath(knob, juce::PathStrokeType(2.0f));
}

void LookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) {
    const auto& p = palette();
    auto r = b.getLocalBounds().toFloat().reduced(0.5f);
    auto c = b.getToggleState() ? b.findColour(juce::TextButton::buttonOnColourId)
                                : b.findColour(juce::TextButton::buttonColourId);
    if (!b.isEnabled()) c = c.withMultipliedAlpha(0.5f);
    else if (down) c = c.darker(0.12f);
    else if (over) c = c.overlaidWith(p.ink.withAlpha(0.04f));
    g.setColour(c);
    g.fillRoundedRectangle(r, juce::jmin(10.0f, r.getHeight() * 0.5f));
}

juce::Font LookAndFeel::getTextButtonFont(juce::TextButton&, int height) {
    return font(juce::jmin(15.0f, static_cast<float>(height) * 0.5f), true);
}

void LookAndFeel::drawCallOutBoxBackground(juce::CallOutBox&, juce::Graphics& g, const juce::Path& path, juce::Image&) {
    juce::DropShadow(juce::Colours::black.withAlpha(0.18f), 18, { 0, 4 }).drawForPath(g, path);
    g.setColour(palette().card);
    g.fillPath(path);
}

} // namespace ui
