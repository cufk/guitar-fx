#pragma once
// Monokromt formspråk: varm off-white bakgrund, något mörkare rutor, svart text och
// svarta runda rattar med en ljus prick. Mörkt läge inverterar (nästan svart bakgrund,
// ljusa rattar). Platt, luftigt, stora fetstilta rubriker och tunna stora siffror.
// Enda färgen är rött för varningar. Typsnitt: Inter (inbäddat).

#include <juce_gui_basics/juce_gui_basics.h>
#include "engine/Rig.h"

namespace ui {

struct Palette {
    juce::Colour background;   // sidan
    juce::Colour tile;         // rutor (rattar, effekter, piller)
    juce::Colour tileStrong;   // nedtryckt/hovrad ruta
    juce::Colour line;         // tunna linjer och inaktiva streck
    juce::Colour ink, inkInverse, secondaryInk, tertiaryInk;
    juce::Colour knob, knobDot;
    juce::Colour danger { 0xffd8443c };
    // Bakåtkompatibla namn.
    juce::Colour card, subtle, fill, accent, warn, good;
};

const Palette& palette();
void setDarkMode(bool dark);
bool isDarkMode();

// Mått (8-punktsrutnät).
namespace size {
constexpr int page = 32, gap = 12, cardRadius = 20, rowRadius = 18;
}
// Gemensamt rutnät för effektrutor och rattar: alltid fyra lika breda kolumner.
constexpr int kGridColumns = 4, kGridGap = 12;

enum class Weight { Light, Regular, SemiBold, Bold };
juce::Font font(float size, Weight w = Weight::Regular);
inline juce::Font font(float size, bool bold) { return font(size, bold ? Weight::SemiBold : Weight::Regular); }
// Liten rubrik i versaler ("TEMPO"-stilen i StemSplitter).
juce::Font captionFont();
juce::String upper(const juce::String& s);   // versaler inkl. å/ä/ö oberoende av locale

// Platt ruta (ingen skugga – monokromt formspråk).
void drawTile(juce::Graphics& g, juce::Rectangle<float> r, float radius = 18.0f, juce::Colour fill = palette().tile);
inline void drawCard(juce::Graphics& g, juce::Rectangle<float> r, float radius = size::cardRadius,
                     juce::Colour fill = palette().tile, float = 1.0f) { drawTile(g, r, radius, fill); }

inline juce::String u8(const std::string& s) { return juce::String::fromUTF8(s.c_str()); }
juce::String formatValue(const gfx::ParamSpec& spec, float value);

class LookAndFeel final : public juce::LookAndFeel_V4 {
public:
    LookAndFeel();
    void refreshColours();

    juce::Typeface::Ptr getTypefaceForFont(const juce::Font&) override;
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle,
                          juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                          juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    juce::Font getTextButtonFont(juce::TextButton&, int height) override;
    void drawCallOutBoxBackground(juce::CallOutBox&, juce::Graphics&, const juce::Path&, juce::Image&) override;
    int getCallOutBoxBorderSize(const juce::CallOutBox&) override { return 24; }
    float getCallOutBoxCornerSize(const juce::CallOutBox&) override { return 16.0f; }

    void drawPopupMenuBackground(juce::Graphics&, int w, int h) override;
    void drawPopupMenuItem(juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                           bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                           const juce::String& shortcutKeyText, const juce::Drawable* icon,
                           const juce::Colour* textColour) override;
    void drawPopupMenuSectionHeader(juce::Graphics&, const juce::Rectangle<int>&, const juce::String&) override;
    juce::Font getPopupMenuFont() override { return font(14.5f); }
    int getPopupMenuBorderSize() override { return 6; }
    void getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator, int standardHeight,
                                   int& idealWidth, int& idealHeight) override;

    juce::Font getAlertWindowMessageFont() override { return font(14.0f); }
    juce::Font getAlertWindowTitleFont() override { return font(17.0f, Weight::SemiBold); }
    juce::Font getAlertWindowFont() override { return font(14.0f); }
    void drawAlertBox(juce::Graphics&, juce::AlertWindow&, const juce::Rectangle<int>& textArea, juce::TextLayout&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override { return font(14.0f); }
    juce::Font getLabelFont(juce::Label&) override { return font(14.0f); }
    void fillTextEditorBackground(juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline(juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawScrollbar(juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                       int thumbStart, int thumbSize, bool over, bool down) override;
    void drawTooltip(juce::Graphics&, const juce::String& text, int w, int h) override;
    juce::Rectangle<int> getTooltipBounds(const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent) override;
};

} // namespace ui
