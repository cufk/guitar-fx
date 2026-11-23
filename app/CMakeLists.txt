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

# Appikon (se Resources/BRAND.md): macOS följer Apples rutnät (rundad ruta med transparent
# kant), iOS är fyrkantig utan transparens (iOS rundar själv).
if(GFX_IOS)
    set(GFX_ICON ${CMAKE_CURRENT_SOURCE_DIR}/Resources/app-icon/ios/icon-ios-light-1024.png)
else()
    set(GFX_ICON ${CMAKE_CURRENT_SOURCE_DIR}/Resources/app-icon/macos/icon-macos-light-1024.png)
endif()

juce_add_plugin(Stagebox
    COMPANY_NAME                  "Flatmate"
    BUNDLE_ID                     "se.flatmate.stagebox"
    PLUGIN_MANUFACTURER_CODE      Fltm
    PLUGIN_CODE                   Gtfx
    PRODUCT_NAME                  "Stagebox"
    ICON_BIG                      ${GFX_ICON}
    FORMATS                       ${GFX_FORMATS}
    IS_SYNTH                      FALSE
    NEEDS_MIDI_INPUT              TRUE      # för fotswitch senare
    NEEDS_MIDI_OUTPUT             FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT    "Stagebox behöver ljudingången för att ta emot gitarrsignalen."
    BACKGROUND_AUDIO_ENABLED      TRUE      # iOS: fortsätt spela om skärmen släcks
    REQUIRES_FULL_SCREEN          TRUE
    # iPad: bara iPad (layouten är för liten för iPhone), liggande, helskärm. Exporterade
    # presets syns i Filer-appen under "På min iPad > Stagebox".
    TARGETED_DEVICE_FAMILY        2
    IPAD_SCREEN_ORIENTATIONS      UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight
    IPHONE_SCREEN_ORIENTATIONS    UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight
    STATUS_BAR_HIDDEN             TRUE
    FILE_SHARING_ENABLED          TRUE
    DOCUMENT_BROWSER_ENABLED      FALSE
    COPY_PLUGIN_AFTER_BUILD       FALSE)

# Inbäddade resurser: typsnitt (Inter, SIL Open Font License – se Resources/fonts/Inter-LICENSE.txt)
# och logotypen för sidhuvudet (ljus/mörk, SVG – varianten i logo/app/ med förberäknat märke).
juce_add_binary_data(StageboxResources SOURCES
    Resources/fonts/Inter-Light.ttf
    Resources/fonts/Inter-Regular.ttf
    Resources/fonts/Inter-SemiBold.ttf
    Resources/fonts/Inter-Bold.ttf
    Resources/logo/app/stagebox-logo-light.svg
    Resources/logo/app/stagebox-logo-dark.svg)

target_sources(Stagebox PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/StandaloneApp.cpp
    Source/ui/Icons.cpp
    Source/ui/Theme.cpp
    Source/ui/Widgets.cpp)

target_compile_definitions(Stagebox PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)   # eget appfönster, se StandaloneApp.cpp

target_link_libraries(Stagebox
    PRIVATE
        gfx_core
        StageboxResources
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
