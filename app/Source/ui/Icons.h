#pragma once
// Vektorikoner (ritade med Path, i stället för SF Symbols): en per effekt plus
// några för knappar. Ritas i given rektangel med given färg.

#include <juce_gui_basics/juce_gui_basics.h>

namespace ui::icons {

void drawEffect(juce::Graphics& g, const std::string& effectId, juce::Rectangle<float> r, juce::Colour c);

enum class Symbol { Sliders, More, Plus, Prev, Next, ChevronDown, ChevronUp, Check, Speaker, Waveform, ArrowUp, Dot, Particles };
void draw(juce::Graphics& g, Symbol s, juce::Rectangle<float> r, juce::Colour c, float strokeWidth = 1.8f);

} // namespace ui::icons
