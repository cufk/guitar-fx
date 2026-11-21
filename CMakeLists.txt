cmake_minimum_required(VERSION 3.22)

project(GuitarFX VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Lägsta macOS-version")
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        set(GFX_IOS ON)
    endif()
endif()

option(GFX_BUILD_APP   "Bygg JUCE-appen (Standalone/AU/VST3/AUv3)" ON)
option(GFX_BUILD_TOOLS "Bygg offline-render-verktyget"             ON)
option(GFX_BUILD_TESTS "Bygg testerna"                              ON)

if(GFX_IOS)
    # Kommandoradsverktyg och tester körs inte på iOS.
    set(GFX_BUILD_TOOLS OFF)
    set(GFX_BUILD_TESTS OFF)
endif()

# ---------------------------------------------------------------------------
# Portabel kärna: DSP + motor. Inga beroenden utöver C++-standardbiblioteket.
# ---------------------------------------------------------------------------
add_library(gfx_core STATIC
    engine/EffectChain.cpp
    engine/Engine.cpp
    dsp/effects/Cabinet.cpp
)
target_include_directories(gfx_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
if(MSVC)
    target_compile_options(gfx_core PRIVATE /W4)
else()
    target_compile_options(gfx_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
set_target_properties(gfx_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(GFX_BUILD_TOOLS)
    add_subdirectory(tools/offline-render)
endif()

if(GFX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(GFX_BUILD_APP)
    add_subdirectory(app)
endif()
