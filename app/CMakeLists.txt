# JUCE-appen: tunt skal runt gfx::Engine.
# Samma kod byggs som fristående app (Mac/iOS/Linux) och som plugin (AU/VST3/AUv3).

include(FetchContent)

# Använd en lokal JUCE-kopia om JUCE_DIR är satt, annars hämta en låst version.
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
else()
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        9.0.6
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(GFX_IOS)
    set(GFX_FORMATS Standalone AUv3)
elseif(APPLE)
    set(GFX_FORMATS Standalone AU VST3)
else()
    set(GFX_FORMATS Standalone VST3)   # Linux / Raspberry Pi
endif()

juce_add_plugin(GuitarFX
    COMPANY_NAME                  "Flatmate"
    BUNDLE_ID                     "se.flatmate.guitarfx"
    PLUGIN_MANUFACTURER_CODE      Fltm
    PLUGIN_CODE                   Gtfx
    PRODUCT_NAME                  "GuitarFX"
    FORMATS                       ${GFX_FORMATS}
    IS_SYNTH                      FALSE
    NEEDS_MIDI_INPUT              TRUE      # för fotswitch senare
    NEEDS_MIDI_OUTPUT             FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT    "GuitarFX behöver ljudingången för att ta emot gitarrsignalen."
    BACKGROUND_AUDIO_ENABLED      TRUE      # iOS: fortsätt spela om skärmen släcks
    REQUIRES_FULL_SCREEN          TRUE
    COPY_PLUGIN_AFTER_BUILD       FALSE)

# Typsnitt (Inter, SIL Open Font License – se Resources/fonts/Inter-LICENSE.txt).
juce_add_binary_data(GuitarFXFonts SOURCES
    Resources/fonts/Inter-Light.ttf
    Resources/fonts/Inter-Regular.ttf
    Resources/fonts/Inter-SemiBold.ttf
    Resources/fonts/Inter-Bold.ttf)

target_sources(GuitarFX PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/StandaloneApp.cpp
    Source/ui/Icons.cpp
    Source/ui/Theme.cpp
    Source/ui/Widgets.cpp)

target_compile_definitions(GuitarFX PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)   # eget appfönster, se StandaloneApp.cpp

target_link_libraries(GuitarFX
    PRIVATE
        gfx_core
        GuitarFXFonts
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
