#include "Theme.h"
#include "BinaryData.h"
#include <cmath>

namespace ui {

namespace {
Palette makePalette(bool dark) {
    Palette p;
    auto c = [](juce::uint32 argb) { return juce::Colour(argb); };
    if (dark) {
        p.background = c(0xff121212); p.tile = c(0xff1e1e1d); p.tileStrong = c(0xff2a2a28); p.line = c(0xff333331);
        p.ink = c(0xffedeae3); p.inkInverse = c(0xff121212); p.secondaryInk = c(0xff9a968e); p.tertiaryInk = c(0xff5f5c57);
        p.knob = c(0xffedeae3); p.knobDot = c(0xff121212);
    } else {
        p.background = c(0xffe8e5de); p.tile = c(0xffdad7cf); p.tileStrong = c(0xffcdc9c0); p.line = c(0xffc9c5bc);
        p.ink = c(0xff111111); p.inkInverse = c(0xfff4f2ec); p.secondaryInk = c(0xff6e6b65); p.tertiaryInk = c(0xffa29e96);
        p.knob = c(0xff121212); p.knobDot = c(0xfff4f2ec);
    }
    p.card = p.tile; p.subtle = p.tileStrong; p.fill = p.line; p.accent = p.ink; p.warn = p.ink; p.good = p.ink;
    return p;
}
bool gDark = false;
Palette gPalette = makePalette(false);

struct Typefaces {
    juce::Typeface::Ptr light, regular, semibold, bold;
    Typefaces() {
        light    = juce::Typeface::createSystemTypefaceFor(BinaryData::InterLight_ttf,    BinaryData::InterLight_ttfSize);
        regular  = juce::Typeface::createSystemTypefaceFor(BinaryData::InterRegular_ttf,  BinaryData::InterRegular_ttfSize);
        semibold = juce::Typeface::createSystemTypefaceFor(BinaryData::InterSemiBold_ttf, BinaryData::InterSemiBold_ttfSize);
        bold     = juce::Typeface::createSystemTypefaceFor(BinaryData::InterBold_ttf,     BinaryData::InterBold_ttfSize);
    }
};
const Typefaces& typefaces() { static Typefaces t; return t; }
} // namespace

const Palette& palette() { return gPalette; }
void setDarkMode(bool dark) { gDark = dark; gPalette = makePalette(dark); }
bool isDarkMode() { return gDark; }

juce::Font font(float size, Weight w) {
    const auto& t = typefaces();
    auto tf = w == Weight::Bold ? t.bold : w == Weight::SemiBold ? t.semibold : w == Weight::Light ? t.light : t.regular;
    return juce::Font(juce::FontOptions(tf).withHeight(size));
}

juce::Font captionFont() { return font(10.5f, Weight::SemiBold).withExtraKerningFactor(0.07f); }

juce::String upper(const juce::String& s) {
    return s.toUpperCase().replace(juce::String::fromUTF8("å"), juce::String::fromUTF8("Å"))
                          .replace(juce::String::fromUTF8("ä"), juce::String::fromUTF8("Ä"))
                          .replace(juce::String::fromUTF8("ö"), juce::String::fromUTF8("Ö"));
}

void drawTile(juce::Graphics& g, juce::Rectangle<float> r, float radius, juce::Colour fill) {
    g.setColour(fill);
    g.fillRoundedRectangle(r, radius);
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
        if (s.startsWithChar('-') && std::abs(s.getDoubleValue()) < 1e-9) s = s.substring(1);
        if (!spec.unit.empty()) s << " " << u8(spec.unit);
    }
    return s.replaceCharacter('.', ',').replaceCharacter('-', static_cast<juce::juce_wchar>(0x2212));  // decimalkomma, äkta minus
}

// ---------------------------------------------------------------------------
LookAndFeel::LookAndFeel() { refreshColours(); }

juce::Typeface::Ptr LookAndFeel::getTypefaceForFont(const juce::Font& f) {
    // JUCE:s inbyggda komponenter (menyer, dialoger, ljudinställningar) får också Inter.
    const auto& t = typefaces();
    if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName() || f.getTypefaceName().isEmpty())
        return f.isBold() ? t.semibold : t.regular;
    return juce::LookAndFeel_V4::getTypefaceForFont(f);
}

void LookAndFeel::refreshColours() {
    const auto& p = palette();
    auto scheme = getCurrentColourScheme();
    scheme.setUIColour(ColourScheme::windowBackground, p.card);
    scheme.setUIColour(ColourScheme::widgetBackground, p.subtle);
    scheme.setUIColour(ColourScheme::menuBackground, p.card);
    scheme.setUIColour(ColourScheme::outline, p.fill);
    scheme.setUIColour(ColourScheme::defaultText, p.ink);
    scheme.setUIColour(ColourScheme::defaultFill, p.accent);
    scheme.setUIColour(ColourScheme::highlightedText, juce::Colours::white);
    scheme.setUIColour(ColourScheme::highlightedFill, p.accent);
    scheme.setUIColour(ColourScheme::menuText, p.ink);
    setColourScheme(scheme);

    setColour(juce::ResizableWindow::backgroundColourId, p.background);
    setColour(juce::DocumentWindow::backgroundColourId, p.background);
    setColour(juce::Label::textColourId, p.ink);
    setColour(juce::TextButton::buttonColourId, p.subtle);
    setColour(juce::TextButton::buttonOnColourId, p.ink);
    setColour(juce::TextButton::textColourOffId, p.ink);
    setColour(juce::TextButton::textColourOnId, p.inkInverse);
    setColour(juce::ToggleButton::textColourId, p.ink);
    setColour(juce::ToggleButton::tickColourId, p.accent);
    setColour(juce::ComboBox::backgroundColourId, p.subtle);
    setColour(juce::ComboBox::textColourId, p.ink);
    setColour(juce::ComboBox::outlineColourId, p.fill);
    setColour(juce::ComboBox::arrowColourId, p.secondaryInk);
    setColour(juce::ListBox::backgroundColourId, p.subtle);
    setColour(juce::ListBox::textColourId, p.ink);
    setColour(juce::PopupMenu::backgroundColourId, p.card);
    setColour(juce::PopupMenu::textColourId, p.ink);
    setColour(juce::PopupMenu::headerTextColourId, p.secondaryInk);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, p.subtle);
    setColour(juce::PopupMenu::highlightedTextColourId, p.ink);
    setColour(juce::AlertWindow::backgroundColourId, p.card);
    setColour(juce::AlertWindow::textColourId, p.ink);
    setColour(juce::AlertWindow::outlineColourId, p.card);
    setColour(juce::TextEditor::backgroundColourId, p.subtle);
    setColour(juce::TextEditor::textColourId, p.ink);
    setColour(juce::TextEditor::highlightColourId, p.accent.withAlpha(0.25f));
    setColour(juce::TextEditor::outlineColourId, p.fill);
    setColour(juce::TextEditor::focusedOutlineColourId, p.accent);
    setColour(juce::ScrollBar::thumbColourId, p.fill);
    setColour(juce::Slider::thumbColourId, p.accent);
    setColour(juce::Slider::trackColourId, p.accent);
    setColour(juce::Slider::backgroundColourId, p.fill);
    setColour(juce::CaretComponent::caretColourId, p.accent);
    setColour(juce::TooltipWindow::backgroundColourId, p.ink);
    setColour(juce::TooltipWindow::textColourId, p.inkInverse);
}

// Rund ratt: fylld cirkel med en liten prick som visar läget (270° vridning).
void LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float startAngle,
                                   float endAngle, juce::Slider& s) {
    const auto& p = palette();
    const float d = static_cast<float>(juce::jmin(w, h));
    auto r = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h))
                 .withSizeKeepingCentre(d, d);
    auto knob = s.isEnabled() ? p.knob : p.tertiaryInk;
    if (s.isMouseButtonDown()) knob = knob.interpolatedWith(p.background, 0.10f);
    else if (s.isMouseOver()) knob = knob.interpolatedWith(p.background, 0.05f);
    g.setColour(knob);
    g.fillEllipse(r);

    const float angle = startAngle + pos * (endAngle - startAngle);
    const float rad = d * 0.5f * 0.68f;
    const auto c = r.getCentre();
    const float dot = juce::jmax(6.0f, d * 0.085f);
    const juce::Point<float> dp(c.x + rad * std::sin(angle), c.y - rad * std::cos(angle));
    g.setColour(p.knobDot);
    g.fillEllipse(dp.x - dot * 0.5f, dp.y - dot * 0.5f, dot, dot);
}

// Smalt reglage: tunn bana, fylld del i reglagets färg och en liten knopp (MiniSlider).
void LookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                   juce::Slider::SliderStyle style, juce::Slider& s) {
    if (style == juce::Slider::LinearVertical) {
        // Lodrätt reglage: smalt spår, svart fyllning nedifrån och ett svart runt grepp.
        const auto& p = palette();
        const float cx = static_cast<float>(x) + static_cast<float>(w) * 0.5f;
        const float top = static_cast<float>(y) - 14.0f, bottom = static_cast<float>(y + h) + 14.0f;
        g.setColour(p.tileStrong);
        g.fillRoundedRectangle(cx - 3.0f, top, 6.0f, bottom - top, 3.0f);
        const float py = juce::jlimit(static_cast<float>(y), static_cast<float>(y + h), pos);
        // Reglage kring noll (t.ex. ±12 dB) fylls från noll: ingen fyllning = neutralt.
        float from = bottom;
        if (s.getMinimum() < 0.0 && s.getMaximum() >= 0.0)
            from = static_cast<float>(y) + static_cast<float>(h) * (1.0f - static_cast<float>(s.valueToProportionOfLength(0.0)));
        g.setColour(s.isEnabled() ? p.ink : p.tertiaryInk);
        g.fillRoundedRectangle(cx - 3.0f, juce::jmin(from, py), 6.0f, std::abs(from - py), 3.0f);
        const float r = s.isMouseButtonDown() ? 15.0f : 14.0f;
        g.setColour(s.isEnabled() ? p.knob : p.tertiaryInk);
        g.fillEllipse(cx - r, py - r, 2.0f * r, 2.0f * r);
        return;
    }
    if (style != juce::Slider::LinearHorizontal) {
        juce::LookAndFeel_V4::drawLinearSlider(g, x, y, w, h, pos, 0, 0, style, s);
        return;
    }
    const auto& p = palette();
    const auto tint = s.isEnabled() ? s.findColour(juce::Slider::thumbColourId) : p.tertiaryInk;
    const float cy = static_cast<float>(y) + static_cast<float>(h) * 0.5f;
    const float left = static_cast<float>(x) + 8.0f, right = static_cast<float>(x + w) - 8.0f;
    const float px = juce::jlimit(left, right, pos);

    g.setColour(p.fill);
    g.fillRoundedRectangle(left, cy - 2.0f, right - left, 4.0f, 2.0f);

    // Bipolära reglage (t.ex. ±12 dB) fylls från mitten.
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float zeroX = bipolar ? left + (right - left) * static_cast<float>(s.valueToProportionOfLength(0.0)) : left;
    g.setColour(tint);
    g.fillRoundedRectangle(juce::jmin(zeroX, px), cy - 2.0f, juce::jmax(4.0f, std::abs(px - zeroX)), 4.0f, 2.0f);
    if (bipolar) { g.setColour(p.tertiaryInk.withAlpha(0.6f)); g.fillRect(zeroX - 0.5f, cy - 5.0f, 1.0f, 10.0f); }

    const bool active = s.isMouseOverOrDragging() && s.isEnabled();
    const float r = active ? 8.0f : 7.0f;
    juce::Path knob;
    knob.addEllipse(px - r, cy - r, 2 * r, 2 * r);
    juce::DropShadow(juce::Colours::black.withAlpha(0.16f), 4, { 0, 1 }).drawForPath(g, knob);
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
    else if (down) c = c.overlaidWith(p.ink.withAlpha(0.10f));
    else if (over) c = c.overlaidWith(p.ink.withAlpha(0.05f));
    g.setColour(c);
    g.fillRoundedRectangle(r, juce::jmin(10.0f, r.getHeight() * 0.5f));
}

juce::Font LookAndFeel::getTextButtonFont(juce::TextButton&, int height) {
    return font(juce::jmin(14.5f, static_cast<float>(height) * 0.48f), Weight::SemiBold);
}

void LookAndFeel::drawCallOutBoxBackground(juce::CallOutBox&, juce::Graphics& g, const juce::Path& path, juce::Image&) {
    juce::DropShadow(juce::Colours::black.withAlpha(gDark ? 0.5f : 0.14f), 28, { 0, 8 }).drawForPath(g, path);
    g.setColour(palette().background);   // rutorna i panelen syns mot sidans bakgrund
    g.fillPath(path);
}

void LookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int w, int h) {
    const auto& p = palette();
    g.fillAll(p.card);
    g.setColour(p.line);
    g.drawRect(0, 0, w, h, 1);
}

void LookAndFeel::getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator, int standardHeight,
                                            int& idealWidth, int& idealHeight) {
    juce::LookAndFeel_V4::getIdealPopupMenuItemSize(text, isSeparator, standardHeight, idealWidth, idealHeight);
    idealHeight = isSeparator ? 9 : 32;
    idealWidth += 24;
}

void LookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                                    bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                                    const juce::String& shortcutKeyText, const juce::Drawable*, const juce::Colour*) {
    const auto& p = palette();
    if (isSeparator) {
        g.setColour(p.line);
        g.fillRect(area.reduced(10, 0).withSizeKeepingCentre(area.getWidth() - 20, 1));
        return;
    }
    auto r = area.reduced(4, 1);
    if (isHighlighted && isActive) {
        g.setColour(p.subtle.overlaidWith(p.ink.withAlpha(0.04f)));
        g.fillRoundedRectangle(r.toFloat(), 8.0f);
    }
    auto textArea = r.reduced(12, 0);
    if (isTicked) {
        auto tick = textArea.removeFromLeft(16).toFloat();
        juce::Path t;
        const auto c = tick.getCentre();
        t.startNewSubPath(c.x - 5, c.y); t.lineTo(c.x - 1.5f, c.y + 4); t.lineTo(c.x + 5, c.y - 4);
        g.setColour(p.accent);
        g.strokePath(t, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        textArea.removeFromLeft(4);
    }
    g.setColour(isActive ? p.ink : p.tertiaryInk);
    g.setFont(getPopupMenuFont());
    g.drawFittedText(text, textArea, juce::Justification::centredLeft, 1);
    if (shortcutKeyText.isNotEmpty()) {
        g.setColour(p.tertiaryInk);
        g.drawText(shortcutKeyText, textArea, juce::Justification::centredRight);
    }
    if (hasSubMenu) {
        juce::Path arrow;
        const float ax = static_cast<float>(textArea.getRight()) - 4.0f, ay = static_cast<float>(textArea.getCentreY());
        arrow.startNewSubPath(ax - 4, ay - 4); arrow.lineTo(ax, ay); arrow.lineTo(ax - 4, ay + 4);
        g.setColour(p.secondaryInk);
        g.strokePath(arrow, juce::PathStrokeType(1.6f));
    }
}

void LookAndFeel::drawPopupMenuSectionHeader(juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& text) {
    g.setColour(palette().secondaryInk);
    g.setFont(captionFont());
    g.drawText(upper(text), area.reduced(16, 0).withTrimmedTop(6), juce::Justification::centredLeft);
}

void LookAndFeel::drawAlertBox(juce::Graphics& g, juce::AlertWindow& w, const juce::Rectangle<int>& textArea, juce::TextLayout& layout) {
    g.fillAll(palette().card);
    layout.draw(g, textArea.toFloat());
    juce::ignoreUnused(w);
}

void LookAndFeel::fillTextEditorBackground(juce::Graphics& g, int w, int h, juce::TextEditor& e) {
    g.setColour(e.findColour(juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle(0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h), 8.0f);
}

void LookAndFeel::drawTextEditorOutline(juce::Graphics& g, int w, int h, juce::TextEditor& e) {
    if (!e.isEnabled()) return;
    g.setColour(e.hasKeyboardFocus(true) ? palette().accent : palette().fill);
    g.drawRoundedRectangle(0.5f, 0.5f, static_cast<float>(w) - 1.0f, static_cast<float>(h) - 1.0f, 8.0f,
                           e.hasKeyboardFocus(true) ? 2.0f : 1.0f);
}

void LookAndFeel::drawScrollbar(juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                                int thumbStart, int thumbSize, bool over, bool down) {
    auto thumb = vertical ? juce::Rectangle<int>(x, thumbStart, w, thumbSize) : juce::Rectangle<int>(thumbStart, y, thumbSize, h);
    g.setColour(palette().tertiaryInk.withAlpha(over || down ? 0.6f : 0.35f));
    g.fillRoundedRectangle(thumb.toFloat().reduced(1.5f), 3.0f);
}

juce::Rectangle<int> LookAndFeel::getTooltipBounds(const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent) {
    const int w = juce::jmin(320, static_cast<int>(juce::GlyphArrangement::getStringWidth(font(13.0f), text)) + 24);
    const int lines = juce::jmax(1, static_cast<int>(juce::GlyphArrangement::getStringWidth(font(13.0f), text)) / 296 + 1);
    const int h = 14 + lines * 18;
    return juce::Rectangle<int>(pos.x - w / 2, pos.y + 18, w, h).constrainedWithin(parent);
}

void LookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text, int w, int h) {
    g.setColour(palette().ink.withAlpha(0.92f));
    g.fillRoundedRectangle(0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h), 8.0f);
    g.setColour(palette().inkInverse);
    g.setFont(font(13.0f));
    g.drawFittedText(text, juce::Rectangle<int>(w, h).reduced(12, 7), juce::Justification::centredLeft, 4);
}

} // namespace ui
