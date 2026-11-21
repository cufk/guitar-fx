#pragma once
// Formspråk från StemSplitter: ljus neutral bakgrund, vita kort med mjuk skugga,
// svart och orange som accenter. Mörkt läge följer systemet.

#include <juce_gui_basics/juce_gui_basics.h>
#include "engine/Rig.h"

namespace ui {

struct Palette {
    juce::Colour background, card, subtle, fill, ink, inkInverse, secondaryInk;
    juce::Colour accent { 0xffed5429 };        // StemSplitter-orange
    juce::Colour danger { 0xffe5484d }, warn { 0xfff5a524 }, good { 0xff2fb57b };
};

const Palette& palette();
void setDarkMode(bool dark);
bool isDarkMode();

constexpr float kRadius = 14.0f;

// Vitt kort med mjuk skugga.
void drawCard(juce::Graphics& g, juce::Rectangle<float> r, float radius = kRadius,
              juce::Colour fill = palette().card);

juce::Font font(float size, bool bold = false);
// Liten rubrik i versaler ("TEMPO"-stilen i StemSplitter).
juce::Font captionFont();

inline juce::String u8(const std::string& s) { return juce::String::fromUTF8(s.c_str()); }
juce::String formatValue(const gfx::ParamSpec& spec, float value);

class LookAndFeel final : public juce::LookAndFeel_V4 {
public:
    LookAndFeel();
    void refreshColours();

    void drawLinearSlider(juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                          juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    juce::Font getTextButtonFont(juce::TextButton&, int height) override;
    void drawCallOutBoxBackground(juce::CallOutBox&, juce::Graphics&, const juce::Path&, juce::Image&) override;
    juce::Font getPopupMenuFont() override { return font(15.0f); }
    juce::Font getAlertWindowMessageFont() override { return font(14.0f); }
    juce::Font getAlertWindowTitleFont() override { return font(17.0f, true); }
};

} // namespace ui
