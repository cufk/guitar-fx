#pragma once
// GUI-komponenter i StemSplitter-stil. Allt läser och skriver motorn via
// gfx::Parameter / EffectChain / Rig (meddelandetråden) – inget ljud här.

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "../PluginProcessor.h"

namespace ui {

// Rund vit knapp med symbol (CircleIconButton).
class IconButton final : public juce::Button {
public:
    enum class Icon { Sliders, More, Plus, Prev, Next, ChevronDown, ChevronUp };
    IconButton(Icon icon, const juce::String& tooltip);
    void setIcon(Icon i) { icon_ = i; repaint(); }
    void paintButton(juce::Graphics&, bool over, bool down) override;
    bool filled = true;   // false = bara symbolen (för små knappar i rader)
private:
    Icon icon_;
};

// Liten av/på-knapp med text (ToggleChip).
class Chip final : public juce::Button {
public:
    Chip(const juce::String& text, juce::Colour onColour);
    void setOnColour(juce::Colour c) { onColour_ = c; repaint(); }
    void paintButton(juce::Graphics&, bool over, bool down) override;
private:
    juce::Colour onColour_;
};

// Ett reglage: rubrik i versaler, värde till höger och MiniSlider under.
// Val (choices) visas som chips i en rad.
class ParamControl final : public juce::Component {
public:
    ParamControl(const gfx::ParamSpec& spec, juce::Colour tint);
    void refresh();
    void paint(juce::Graphics&) override;
    void resized() override;
    static constexpr int kHeight = 50;
private:
    gfx::ParamSpec spec_;
    juce::Slider slider_;
    std::vector<std::unique_ptr<Chip>> choices_;
};

// En effekt som rad (som ett spår i StemSplitter): färg, namn, status, PÅ / I PRESET,
// och reglagen när raden är utfälld.
class EffectRow final : public juce::Component {
public:
    EffectRow(GuitarFXProcessor& proc, const gfx::EffectSpec& spec, std::function<void()> onLayoutChange);
    int preferredHeight(int width) const;
    void refresh();
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent&) override;
    void setExpanded(bool e);
    bool isExpanded() const { return expanded_; }
private:
    int columns(int width) const { return width >= 560 ? 3 : 2; }
    GuitarFXProcessor& proc_;
    const gfx::EffectSpec& spec_;
    std::function<void()> onLayoutChange_;
    Chip onChip_, presetChip_;
    IconButton expand_ { IconButton::Icon::ChevronDown, juce::String::fromUTF8("Visa reglage") };
    std::vector<std::unique_ptr<ParamControl>> params_;
    // Kabinett: IR-val.
    std::unique_ptr<juce::TextButton> loadIr_, builtInIr_;
    std::unique_ptr<juce::FileChooser> chooser_;
    juce::String irStatus_;
    bool expanded_ = false;
    static constexpr int kHeader = 60;
};

// Rullbar lista med effektrader.
class EffectList final : public juce::Component {
public:
    explicit EffectList(GuitarFXProcessor& proc);
    void refresh() { for (auto& r : rows_) r->refresh(); }
    void resized() override;
private:
    void layoutRows();
    juce::Viewport viewport_;
    juce::Component content_;
    std::vector<std::unique_ptr<EffectRow>> rows_;
};

// Presets som stora knappar (fotswitch-känsla). Tryck = aktivera, igen = grundljud.
// Högerklick / långt tryck = meny (byt namn, duplicera, flytta, ta bort).
class PresetPanel final : public juce::Component {
public:
    explicit PresetPanel(GuitarFXProcessor& proc);
    void refresh();
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent&) override;
private:
    juce::Rectangle<int> tileBounds(int tile) const;  // tile 0 = grundljud, 1.. = presets
    int tileAt(juce::Point<int> p) const;
    void showMenu(int presetIndex);
    void rename(int presetIndex);
    GuitarFXProcessor& proc_;
    IconButton add_ { IconButton::Icon::Plus, "Nytt preset" };
    juce::Rectangle<int> grid_;
    uint64_t lastRevision_ = ~0ull;
};

// Ingång och nivåer (öppnas från sidhuvudet som popover).
class InputPanel final : public juce::Component, private juce::Timer {
public:
    explicit InputPanel(GuitarFXProcessor& proc);
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    GuitarFXProcessor& proc_;
    gfx::ParamSpec balanceSpec_, alignSpec_, inSpec_, outSpec_;
    Chip src1_, src2_, srcBlend_, inv1_, inv2_;
    ParamControl balance_, align_, inGain_, outGain_;
    juce::TextButton autoAlign_ { "Auto-justera" };
    juce::String alignText_;
    juce::Colour alignColour_;
};

// Sidhuvud: ingångsknapp, titel med undertitel och meny.
class HeaderBar final : public juce::Component {
public:
    HeaderBar();
    void setTexts(const juce::String& title, const juce::String& subtitle);
    void paint(juce::Graphics&) override;
    void resized() override;
    IconButton input { IconButton::Icon::Sliders, juce::String::fromUTF8("Ingång och nivåer") };
    IconButton more  { IconButton::Icon::More, "Fler val" };
private:
    juce::String title_, subtitle_;
};

// Fältet längst ned: nivåmätare, preset-växling och status.
class BottomBar final : public juce::Component {
public:
    explicit BottomBar(GuitarFXProcessor& proc);
    void update(float in1, float in2, float out);
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    GuitarFXProcessor& proc_;
    IconButton prev_ { IconButton::Icon::Prev, juce::String::fromUTF8("Föregående preset (←)") };
    IconButton next_ { IconButton::Icon::Next, juce::String::fromUTF8("Nästa preset (→)") };
    float in1_ = 0, in2_ = 0, out_ = 0;
    juce::Rectangle<int> meters_, centre_, chips_;
};

} // namespace ui
