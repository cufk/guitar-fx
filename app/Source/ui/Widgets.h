#pragma once
// GUI-komponenter i det monokroma formspråket. Allt läser och skriver motorn via
// gfx::Parameter / EffectChain / Rig (meddelandetråden) – inget ljud här.
//
//   HeaderBar      stor rubrik, aktivt läge, ingångsknapp och meny
//   StageView      ring av streck (utnivå) runt aktivt presetnummer, namn, bläddring, mätare
//   EffectGrid     de åtta effekterna som rutor (välj vilken som visas)
//   EffectPanel    vald effekt: på/av, "I preset" och runda rattar
//   PresetBar      presets som piller längst ned
//   InputPanel     ingång/utgång/nivåer (popover)

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <thread>
#include "Icons.h"
#include "Theme.h"
#include "dsp/core/PitchDetector.h"
#include "../PluginProcessor.h"

namespace ui {

// Rund knapp med symbol.
class IconButton final : public juce::Button {
public:
    IconButton(icons::Symbol s, const juce::String& tooltip, bool filled = true);
    void setSymbol(icons::Symbol s) { symbol_ = s; repaint(); }
    void paintButton(juce::Graphics&, bool over, bool down) override;
private:
    icons::Symbol symbol_;
    bool filled_;
};

// Pillerformad knapp. På = svart (inverterad), av = ruta.
class Pill final : public juce::Button {
public:
    explicit Pill(const juce::String& label, bool toggle = true);
    void paintButton(juce::Graphics&, bool over, bool down) override;
    juce::String prefix;            // liten text före (t.ex. presetnummer)
    // Valfria färger (används t.ex. i stämfältet, som har egen bakgrund). Genomskinlig = standard.
    juce::Colour offFill, onFill, offText, onText;
    icons::Symbol symbol = icons::Symbol::More;
    bool showSymbol = false;
};

// Av/på-reglage (monokromt).
class Switch final : public juce::Button, private juce::Timer {
public:
    Switch();
    void paintButton(juce::Graphics&, bool over, bool down) override;
    using juce::Button::clicked;
    void clicked() override { startTimerHz(60); }
private:
    void timerCallback() override;
    float pos_ = 0.0f;
};

// En ratt i en ruta: svart rund ratt, namn nere till vänster och värde till höger.
// Dra uppåt/nedåt eller i sidled; dubbelklick återställer standardvärdet.
// Val (choices) visas som staplade piller i stället för ratt.
class KnobTile final : public juce::Component {
public:
    explicit KnobTile(const gfx::ParamSpec& spec);
    void refresh();
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    gfx::ParamSpec spec_;
    juce::Slider knob_;
    std::vector<std::unique_ptr<Pill>> choices_;
};

// De åtta effekterna som rutor. Prick = på. Svart = vald (visas i EffectPanel).
class EffectGrid final : public juce::Component {
public:
    EffectGrid(GuitarFXProcessor& proc, std::function<void(int)> onSelect);
    void setSelected(int i) { selected_ = i; repaint(); }
    void refresh() { repaint(); }
    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override { hover_ = -1; repaint(); }
private:
    juce::Rectangle<int> cell(int i) const;
    int cellAt(juce::Point<int>) const;
    GuitarFXProcessor& proc_;
    std::function<void(int)> onSelect_;
    int selected_ = 0, hover_ = -1;
};

// Vald effekt: rubrik, status, "I preset", på/av och rattar.
class EffectPanel final : public juce::Component {
public:
    explicit EffectPanel(GuitarFXProcessor& proc);
    void show(int effectIndex);
    void refresh();
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    GuitarFXProcessor& proc_;
    int index_ = -1;
    Switch switch_;
    Pill presetPill_ { "I preset" };
    std::unique_ptr<Pill> loadIr_, builtInIr_;
    std::unique_ptr<juce::FileChooser> chooser_;
    juce::String irStatus_;
    std::vector<std::unique_ptr<KnobTile>> tiles_;
    float activity_ = 0.0f;
};

// Setlist: grundljud + presets i spelordning. Tryck = aktivera, dra en rad = ändra ordning,
// Föregående/Nästa (och fotswitch CC 102/103, ←/→) stegar i den här ordningen.
// Högerklick / långt tryck = byt namn, duplicera, ta bort.
class SetlistPanel final : public juce::Component, private juce::Timer {
public:
    explicit SetlistPanel(GuitarFXProcessor& proc);
    void refresh();
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override { hover_ = -2; repaint(); }
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    IconButton analyse { icons::Symbol::Waveform, juce::String::fromUTF8("Analysera en låt och skapa ett preset") };
private:
    void timerCallback() override;
    int rowAt(int y) const;                           // -1 = grundljud, 0.. = preset, -2 = ingen
    juce::Rectangle<int> rowBounds(int row) const;    // row: 0 = grundljud, 1.. = presets (före scroll)
    void showMenu(int presetIndex);
    void rename(int presetIndex);
    void scrollToActive();
    int maxScroll() const;
    GuitarFXProcessor& proc_;
    IconButton add_ { icons::Symbol::Plus, "Nytt preset" };
    Pill prev_ { juce::String::fromUTF8("‹  Föregående"), false };
    Pill next_ { juce::String::fromUTF8("Nästa  ›"), false };
    juce::Rectangle<int> list_;
    int scroll_ = 0, hover_ = -2;
    int pressRow_ = -2, dragFrom_ = -1, dragTo_ = -1, dragY_ = 0;
    juce::uint32 downTime_ = 0;
    bool popupDown_ = false, dragging_ = false;
    uint64_t lastRevision_ = ~0ull;
    int lastActive_ = -3;
    static constexpr int kRowH = 60, kRowGap = 8;
};

// Kompakt signalindikator: bara de ingångar som används (IN 1, IN 2 eller båda vid Blanda).
class InputMeter final : public juce::Component {
public:
    explicit InputMeter(GuitarFXProcessor& proc) : proc_(proc) {}
    void update();
    void paint(juce::Graphics&) override;
private:
    GuitarFXProcessor& proc_;
    float level_[2] {}, hold_[2] {};
    int holdTicks_[2] {};
};

// Ingång, utgång och nivåer (popover från sidhuvudet).
class InputPanel final : public juce::Component, private juce::Timer {
public:
    explicit InputPanel(GuitarFXProcessor& proc);
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    GuitarFXProcessor& proc_;
    gfx::ParamSpec balanceSpec_, alignSpec_, inSpec_, outSpec_;
    Pill src1_, src2_, srcBlend_, outStereo_, outMono_, inv1_, inv2_;
    Pill autoAlign_ { "Auto-justera", false };
    KnobTile balance_, align_, inGain_, outGain_;
    juce::String alignText_;
    bool alignWarning_ = false;
    juce::Rectangle<int> alignTextArea_;
    std::vector<std::pair<juce::String, int>> captions_;
};

// Stämningar för stämapparaten (låg sträng först, MIDI-nummer). Tom lista = kromatisk.
struct Tuning { const char* name; std::vector<int> strings; };
const std::vector<Tuning>& tunings();

// Stämapparat som ett lågt vågrätt fält längst ned – alltid synlig och alltid igång.
//   ton  |  cent · Hz  |  skala −50 … +50 cent  |  strängar  |  stämning  |  A4  |  Mute
// Läser den torra ingången via TunerTap och kör PitchDetector på GUI-tråden.
// "Mute" stänger av ljudet ut medan man stämmer; då blir hela fältet svart.
class TunerStrip final : public juce::Component, private juce::Timer {
public:
    explicit TunerStrip(GuitarFXProcessor& proc);
    ~TunerStrip() override;
    void toggleMute();
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    GuitarFXProcessor& proc_;
    gfx::PitchDetector detector_;
    std::vector<float> window_;
    double preparedRate_ = 0.0;
    Pill mute_ { "Mute" };
    Pill tuning_ { "Standard", false };
    Pill reference_ { "440 Hz", false };
    float hz_ = 0.0f, cents_ = 0.0f, shownCents_ = 0.0f;
    int midi_ = 0, holdTicks_ = 0, nearest_ = -1;
    std::vector<float> recent_;
    juce::Rectangle<int> noteArea_, scaleArea_, infoArea_, stringsArea_;
};

// Låtanalys (täcker huvudytan): välj en ljudfil, analysen körs i en egen tråd, och
// resultatet kan bli ett nytt preset. Ren signalanalys (engine/SongAnalyzer).
class AnalysisView final : public juce::Component, private juce::Timer {
public:
    explicit AnalysisView(GuitarFXProcessor& proc);
    ~AnalysisView() override;
    void setOpen(bool open);
    bool isOpen() const { return open_; }
    void chooseFile();
    std::function<void()> onClose;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    enum class State { Idle, Working, Done, Failed };
    void timerCallback() override;
    void start(const juce::File& file);
    void stopWorker();
    GuitarFXProcessor& proc_;
    Pill choose_ { juce::String::fromUTF8("Välj låt…"), false };
    Pill create_ { "Skapa preset", false };
    Pill close_ { "Klar", false };
    std::unique_ptr<juce::FileChooser> chooser_;
    std::thread worker_;
    std::atomic<bool> cancel_ { false }, finished_ { false };
    std::atomic<float> progress_ { 0.0f };
    gfx::SongAnalysis result_;      // skrivs av tråden innan finished_ sätts
    juce::String error_;
    State state_ = State::Idle;
    bool open_ = false;
    juce::String title_;
    juce::Rectangle<int> body_;
};

// Sidhuvud: appens namn och menyn (alla inställningar finns under ⋯).
class HeaderBar final : public juce::Component {
public:
    HeaderBar();
    void paint(juce::Graphics&) override;
    void resized() override;
    IconButton more { icons::Symbol::More, juce::String::fromUTF8("Inställningar"), false };
};

} // namespace ui
