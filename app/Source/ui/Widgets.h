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

// Vänster spalt: ringen av streck (utnivå), presetnummer, namn och bläddring.
class StageView final : public juce::Component {
public:
    explicit StageView(GuitarFXProcessor& proc);
    void update(float in1, float in2, float out, const juce::String& latency);
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    GuitarFXProcessor& proc_;
    IconButton prev_ { icons::Symbol::Prev, juce::String::fromUTF8("Föregående preset (←)") };
    IconButton next_ { icons::Symbol::Next, juce::String::fromUTF8("Nästa preset (→)") };
    float level_[3] {}, ring_ = 0.0f, hold_ = 0.0f;
    int holdTicks_ = 0;
    juce::String latency_;
    juce::Rectangle<float> ringArea_;
    juce::Rectangle<int> nameArea_, metersArea_, statusArea_;
};

// Presets som piller längst ned. Tryck = aktivera, igen = grundljud.
// Högerklick / långt tryck = byt namn, duplicera, flytta, ta bort.
class PresetBar final : public juce::Component {
public:
    explicit PresetBar(GuitarFXProcessor& proc);
    void refresh();
    void resized() override;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
private:
    bool isMenuGesture() const;
    void rebuild();
    void showMenu(int presetIndex);
    void rename(int presetIndex);
    GuitarFXProcessor& proc_;
    std::vector<std::unique_ptr<Pill>> pills_;   // 0 = grundljud
    IconButton add_ { icons::Symbol::Plus, "Nytt preset" };
    uint64_t lastRevision_ = ~0ull;
    size_t lastCount_ = ~size_t(0);
    juce::uint32 downTime_ = 0;
    bool popupDown_ = false;
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

// Stämapparat (täcker huvudytan när den är öppen). Läser ingången via TunerTap och
// kör PitchDetector på GUI-tråden. Stora tecken så att den går att läsa på avstånd.
class TunerView final : public juce::Component, private juce::Timer {
public:
    explicit TunerView(GuitarFXProcessor& proc);
    void setOpen(bool open);
    bool isOpen() const { return open_; }
    std::function<void()> onClose;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    GuitarFXProcessor& proc_;
    gfx::PitchDetector detector_;
    std::vector<float> window_;
    Pill mute_ { "Tysta ut" };
    Pill reference_ { "A4 = 440 Hz", false };
    Pill close_ { "Klar", false };
    std::vector<std::unique_ptr<Pill>> strings_;
    bool open_ = false;
    float referenceA4_ = 440.0f;
    // Utjämnat resultat.
    float hz_ = 0.0f, cents_ = 0.0f, shownCents_ = 0.0f;
    int midi_ = 0, holdTicks_ = 0;
    std::vector<float> recent_;
    juce::Rectangle<float> arcArea_;
    juce::Rectangle<int> noteArea_, infoArea_;
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

// Sidhuvud: stor rubrik med aktivt läge, ingångspiller och meny.
class HeaderBar final : public juce::Component {
public:
    HeaderBar();
    void setTexts(const juce::String& subtitle, const juce::String& sourceText);
    void paint(juce::Graphics&) override;
    void resized() override;
    Pill input { juce::String::fromUTF8("Ingång 1"), false };
    Pill tune { juce::String::fromUTF8("Stäm"), false };
    Pill analyse { juce::String::fromUTF8("Analysera låt"), false };
    IconButton more { icons::Symbol::More, "Fler val", false };
private:
    juce::String subtitle_;
};

} // namespace ui
