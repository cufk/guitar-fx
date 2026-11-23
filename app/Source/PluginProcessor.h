#pragma once
// JUCE-adapter: kopplar JUCE:s ljudcallback till gfx::Engine.
// All ljudlogik ligger i engine/ och dsp/ – här finns bara limkod, sparning
// och styrning (MIDI-fotswitch -> presets).

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "engine/Engine.h"
#include "engine/Rig.h"
#include "engine/SongAnalyzer.h"
#include "dsp/effects/Cabinet.h"
#include "dsp/effects/Chorus.h"
#include "dsp/effects/Distortion.h"
#include "dsp/effects/Eq.h"
#include "dsp/effects/Flanger.h"
#include "dsp/effects/Reverb.h"
#include "dsp/effects/Sustain.h"
#include "dsp/effects/Widener.h"
#include "dsp/effects/Body.h"
#include "dsp/effects/Delay.h"
#include "dsp/effects/Doubler.h"
#include "dsp/effects/FeedbackSuppressor.h"
#include "dsp/effects/Tremolo.h"
#include "dsp/effects/Volume.h"
#include "engine/TapTempo.h"

class GuitarFXProcessor final : public juce::AudioProcessor, private juce::Timer {
public:
    GuitarFXProcessor();
    ~GuitarFXProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Stagebox"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; } // reverb (max efterklang)

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    gfx::Engine& engine() { return engine_; }
    gfx::Rig& rig() { return rig_; }
    double cpuLoad() const { return loadMeasurer_.getLoadAsProportion(); }
    gfx::Cabinet& cabinet() { return *cabinet_; }
    gfx::FeedbackSuppressor& feedbackSuppressor() { return *feedback_; }
    gfx::Delay& delay() { return *delay_; }
    gfx::Volume& volume() { return *volume_; }

    // Tap tempo (meddelandetråden): sätter delayns tempo. Även MIDI CC 107 och mellanslag.
    void tapTempo();
    // Tidpunkt (ms, Time::getMillisecondCounterHiRes) för senaste tryck – för att blinka i takt.
    double lastTapMs() const { return lastTapMs_.load(std::memory_order_relaxed); }
    // Solo-boost (global, följer inte presets). Även MIDI CC 106 och tangenten B.
    void toggleBoost() { volume_->setBoost(!volume_->boostOn()); }

    // Läs in en IR-fil (WAV/AIFF) till kabinettet. Meddelandetråden.
    // Returnerar ett felmeddelande, eller tom sträng vid lyckad laddning.
    juce::String loadImpulseResponse(const juce::File& file);
    void useBuiltInImpulseResponse();
    juce::File impulseResponseFile() const { return irFile_; }

    // Presets som fil (backup/flytta mellan Mac och iPad).
    juce::var rigToJson() const;
    bool rigFromJson(const juce::var& json);

    // Skapa ett preset från en låtanalys (meddelandetråden). Returnerar presetets index.
    int createPresetFromAnalysis(const gfx::SongAnalysis& a, const juce::String& name);

    // Senast mottagna MIDI-meddelande (för att visa i GUI:t), t.ex. "PC 3".
    juce::String lastMidiDescription() const;

    // Utseende: 0 = följ systemet, 1 = ljust, 2 = mörkt (sparas i state).
    std::atomic<int> appearance { 0 };
    // Stämapparat: vald stämning (index i ui::tunings()) och referens A4 (sparas i state).
    std::atomic<int> tuning { 0 };
    std::atomic<float> referenceA4 { 440.0f };

private:
    void timerCallback() override;
    void createFactoryPresets();

    gfx::Engine engine_;
    gfx::Rig rig_ { engine_.chain() };
    gfx::Eq* eq_ = nullptr;
    gfx::Distortion* distortion_ = nullptr;
    gfx::Cabinet* cabinet_ = nullptr;
    gfx::Chorus* chorus_ = nullptr;
    gfx::Flanger* flanger_ = nullptr;
    gfx::Reverb* reverb_ = nullptr;
    gfx::Sustain* sustain_ = nullptr;
    gfx::Widener* widener_ = nullptr;
    gfx::Body* body_ = nullptr;
    gfx::FeedbackSuppressor* feedback_ = nullptr;
    gfx::Doubler* doubler_ = nullptr;
    gfx::Tremolo* tremolo_ = nullptr;
    gfx::Delay* delay_ = nullptr;
    gfx::Volume* volume_ = nullptr;
    gfx::TapTempo tap_;
    std::atomic<double> lastTapMs_ { 0.0 };
    std::atomic<int> midiTaps_ { 0 };        // tap från MIDI (ljudtråden räknar upp)
    std::atomic<double> midiTapMs_ { 0.0 };
    int handledMidiTaps_ = 0;
    juce::File irFile_;
    juce::AudioProcessLoadMeasurer loadMeasurer_;

    // MIDI -> preset: ljudtråden skriver ett kommando, en timer på
    // meddelandetråden utför det (Rig är inte realtidssäker).
    enum : int { kCmdNone = -1000, kCmdBase = -1, kCmdNext = -2, kCmdPrev = -3 };
    std::atomic<int> pendingCommand_ { kCmdNone };
    std::atomic<int> lastMidi_ { -1 };   // kodat: typ << 16 | nummer << 8 | värde

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarFXProcessor)
};
