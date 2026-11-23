#pragma once
// GUI-komponenter i det monokroma formspråket. Allt läser och skriver motorn via
// gfx::Parameter / EffectChain / Rig (meddelandetråden) – inget ljud här.
//
//   HeaderBar      appens namn och ⋯ (alla inställningar)
//   InputMeter     nivåmätare i sidhuvudet, bara använda ingångar
//   SetlistPanel   grundljud + presets i spelordning, Föregående/Nästa
//   EffectGrid     "Effekter": de åtta effekterna som rutor i signalordning (svart = på)
//   EffectPanel    vald effekt: rubrik + status, "I presetet", på/av och lodräta reglage
//   InputPanel     ingång/utgång/nivåer (popover)
//   TunerStrip     stämapparat längst ned
// Formspråk: rubriker 22 px på samma linje, knappar 44 px (små 34 px), rutor med radie 16–20.


#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <thread>
#include "Icons.h"
#include "Theme.h"
#include "dsp/core/PitchDetector.h"
#include "../PluginProcessor.h"

namespace ui {
// Fil vald i en FileChooser som en läsbar lokal fil. På Mac: den valda filen. På iPad ger
// Filer-appen bara en tillfällig URL – då kopieras innehållet till destDir (som skapas).
// Tom File om inget valdes eller om läsningen misslyckades.
juce::File localCopyOf(const juce::FileChooser& fc, const juce::File& destDir);
// Appens egen datamapp (presets-export, kopierade IR:er m.m.).
juce::File appDataDirectory();

// Sektionsrubrik: "Setlist  1 av 3" – rubrik och en grå rad på samma baslinje.
void drawSectionHeading(juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, const juce::String& detail);


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
    juce::String suffix;            // grå text efter (t.ex. "+4 dB", "96 BPM")
    float symbolAlpha = 1.0f;       // t.ex. blinkande takt-prick
    float fontScale = 1.0f;         // < 1 i smala rutor (sätts av KnobTile för alla val samtidigt)
    bool chevron = false;           // pil nedåt efter texten = öppnar en meny (rullgardin)
    // Bredd som rymmer prefix, given etikett, suffix och pil (34/44 px höga knappar).
    int idealWidth(const juce::String& label) const;
    // Textbredd vid fontScale 1 (för att räkna ut en gemensam skala).
    float naturalTextWidth() const;
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

// Ett reglage i en ruta. Lodrätt (standard): spår som fyller rutans höjd, svart runt grepp,
// namn och värde centrerat under. Runt (InputPanel): ratt med namn och värde under.
// Dubbelklick återställer standardvärdet. Val (choices) visas som staplade piller.
class KnobTile final : public juce::Component {
public:
    explicit KnobTile(const gfx::ParamSpec& spec, bool vertical = true);
    void refresh();
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    gfx::ParamSpec spec_;
    bool vertical_;
    juce::Slider knob_;
    std::vector<std::unique_ptr<Pill>> choices_;
};

// "Effekter": de åtta effekterna som rutor i signalordning. Svart ruta = på.
// Klick väljer vilken effekt som visas i EffectPanel (index = plats i rig.effects()).
class EffectGrid final : public juce::Component {
public:
    EffectGrid(GuitarFXProcessor& proc, std::function<void(int)> onSelect);
    void setSelected(int i) { selected_ = i; repaint(); }
    static int preferredHeight() { return size::sectionHead + size::sectionGap + 2 * kTileH + kGridGap; }
    void refresh() { repaint(); }
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override { hover_ = -1; repaint(); }
private:
    // Högerklick / långt tryck: ta med effekten i aktivt preset eller låt den följa grundljudet.
    void showMenu(int effectIndex);
    juce::Rectangle<int> cell(int position) const;
    int cellAt(juce::Point<int>) const;     // effektindex eller -1
    GuitarFXProcessor& proc_;
    std::function<void(int)> onSelect_;
    std::vector<int> order_;                // visningsordning (effektindex sorterade efter plats i kedjan)
    int selected_ = 0, hover_ = -1;
    juce::uint32 downTime_ = 0;
    bool popupDown_ = false;
    static constexpr int kTileH = 84;
};

// Rundgångsskyddets aktiva filter: en stapel per plats (djup) med frekvens under.
class FilterView final : public juce::Component {
public:
    explicit FilterView(GuitarFXProcessor& proc) : proc_(proc) {}
    void paint(juce::Graphics&) override;
private:
    GuitarFXProcessor& proc_;
};

// Vald effekt: rubrik, status, "I presetet", på/av och reglage.
// Delay: Tap-knapp med tempo (pricken blinkar i takt). Rundgångsskydd: Nollställ + filtervy.
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
    std::unique_ptr<Pill> loadIr_, builtInIr_;
    Pill tap_ { "Tap", false };
    Pill resetFilters_ { juce::String::fromUTF8("Nollställ"), false };
    FilterView filters_;
    std::unique_ptr<juce::FileChooser> chooser_;
    juce::String irStatus_;
    std::vector<std::unique_ptr<KnobTile>> tiles_;
    // Val (t.ex. kroppstyp, indelning) som rullgardiner till vänster om av/på-reglaget.
    std::vector<std::unique_ptr<Pill>> choiceMenus_;
    std::vector<gfx::ParamSpec> choiceSpecs_;
    int headerLeft_ = 0;          // vänsterkant för knapparna i rubrikraden (för rubrikens bredd)
    float activity_ = 0.0f;
    static constexpr int kHead = 12 + 1 + 15 + size::sectionHead;   // linje + luft + rubrikrad
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
    juce::Rectangle<int> head_;
    juce::Rectangle<int> list_;
    int scroll_ = 0, hover_ = -2;
    int pressRow_ = -2, dragFrom_ = -1, dragTo_ = -1, dragY_ = 0;
    juce::uint32 downTime_ = 0;
    bool popupDown_ = false, dragging_ = false;
    uint64_t lastRevision_ = ~0ull;
    int lastActive_ = -3;
    static constexpr int kRowH = 60, kRowGap = 8;
};

// Kompakt signalindikator i en rundad ruta: bara de ingångar som används
// (IN 1, IN 2 eller båda bredvid varandra vid Blanda). Högerställd i sina gränser.
class InputMeter final : public juce::Component {
public:
    explicit InputMeter(GuitarFXProcessor& proc) : proc_(proc) { setInterceptsMouseClicks(false, false); }
    void update();
    void paint(juce::Graphics&) override;
    int pillWidth() const;      // bredden på den rundade rutan (beror på ingångsläget)
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
    IconButton more { icons::Symbol::More, juce::String::fromUTF8("Inställningar") };
    Pill boost { "Boost" };
private:
    std::unique_ptr<juce::Drawable> logo_;
    bool logoDark_ = false;
};

} // namespace ui
