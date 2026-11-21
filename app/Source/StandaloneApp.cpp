// Eget appfönster för den fristående appen (i stället för JUCE:s standardfönster).
//  - macOS egen titelrad, ingen "Options"-knapp och ingen gul "Audio input is muted"-rad
//    (limitern skyddar utgången; ljudinställningar finns i appens ⋯-meny)
//  - MIDI-ingångar (även Bluetooth-pedaler) öppnas automatiskt när de dyker upp
// Aktiveras av JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1 i app/CMakeLists.txt.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

#if JucePlugin_Build_Standalone

#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#include "PluginEditor.h"

namespace {

class MainWindow final : public juce::DocumentWindow {
public:
    MainWindow(const juce::String& name, juce::StandalonePluginHolder& holder)
        : DocumentWindow(name, ui::palette().background, DocumentWindow::allButtons), holder_(holder) {
        setUsingNativeTitleBar(true);
        setResizable(true, false);
        if (auto* editor = holder.processor->createEditorAndMakeActive()) {
            setContentOwned(editor, true);
            if (auto* limits = editor->getConstrainer())
                setResizeLimits(limits->getMinimumWidth(), limits->getMinimumHeight(),
                                limits->getMaximumWidth(), limits->getMaximumHeight());
        }
        // Återställ fönstrets plats och storlek från förra gången.
        const auto saved = holder.settings != nullptr ? holder.settings->getValue("windowBounds") : juce::String();
        if (saved.isNotEmpty()) restoreWindowStateFromString(saved);
        else centreWithSize(getWidth(), getHeight());
    }

    ~MainWindow() override {
        if (holder_.settings != nullptr) holder_.settings->setValue("windowBounds", getWindowStateAsString());
        clearContentComponent();
    }

    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }

private:
    juce::StandalonePluginHolder& holder_;
};

class GuitarFXApp final : public juce::JUCEApplication {
public:
    GuitarFXApp() {
        juce::PropertiesFile::Options options;
        options.applicationName     = "GuitarFX";
        options.filenameSuffix      = ".settings";
        options.osxLibrarySubFolder = "Application Support";
       #if JUCE_LINUX || JUCE_BSD
        options.folderName          = "~/.config";
       #endif
        properties_.setStorageParameters(options);
    }

    const juce::String getApplicationName() override    { return "GuitarFX"; }
    const juce::String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise(const juce::String&) override {
        // Stereo in (Scarlett in 1 + 2) och stereo ut.
        juce::StandalonePluginHolder::PluginInOuts io;
        io.numIns = 2;
        io.numOuts = 2;
        const juce::Array<juce::StandalonePluginHolder::PluginInOuts> channels { io };
        holder_ = std::make_unique<juce::StandalonePluginHolder>(properties_.getUserSettings(), false, juce::String(),
                                                                 nullptr, channels, /*autoOpenMidiDevices*/ true);
        holder_->getMuteInputValue().setValue(false);
        window_ = std::make_unique<MainWindow>(getApplicationName(), *holder_);
        window_->setVisible(true);
    }

    void shutdown() override {
        if (holder_ != nullptr) { holder_->savePluginState(); holder_->saveAudioDeviceState(); }
        window_ = nullptr;
        holder_ = nullptr;
        properties_.saveIfNeeded();
    }

    void systemRequestedQuit() override {
        if (holder_ != nullptr) holder_->savePluginState();
        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents()) {
            juce::Timer::callAfterDelay(100, [] {
                if (auto* app = juce::JUCEApplicationBase::getInstance()) app->systemRequestedQuit();
            });
        } else {
            quit();
        }
    }

private:
    juce::ApplicationProperties properties_;
    std::unique_ptr<juce::StandalonePluginHolder> holder_;
    std::unique_ptr<MainWindow> window_;
};

} // namespace

juce::JUCEApplicationBase* juce_CreateApplication();
juce::JUCEApplicationBase* juce_CreateApplication() { return new GuitarFXApp(); }

#if JUCE_IOS
void* juce_GetIOSCustomDelegateClass();
void* juce_GetIOSCustomDelegateClass() { return nullptr; }
#endif

// Ljudinställningar i appens tema (anropas från editorns ⋯-meny).
void guitarfxShowAudioSettings(juce::Component* parent);
void guitarfxShowAudioSettings(juce::Component* parent) {
    auto* holder = juce::StandalonePluginHolder::getInstance();
    if (holder == nullptr) return;
    auto selector = std::make_unique<juce::AudioDeviceSelectorComponent>(
        holder->deviceManager, 1, 2, 1, 2, /*midiIn*/ true, /*midiOut*/ false, /*stereoPairs*/ true, /*hideAdvanced*/ false);
    selector->setSize(520, 480);
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned(selector.release());
    o.dialogTitle = juce::String::fromUTF8("Ljudinställningar");
    o.dialogBackgroundColour = ui::palette().card;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.componentToCentreAround = parent;
    if (auto* dw = o.launchAsync()) {
        if (parent != nullptr) dw->setLookAndFeel(&parent->getLookAndFeel());
    }
}

#else
void guitarfxShowAudioSettings(juce::Component*);
void guitarfxShowAudioSettings(juce::Component*) {}
#endif
