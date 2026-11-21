# CLAUDE.md – GuitarFX

Egen gitarreffektprocessor (chorus, flanger, reverb, distortion). Utvecklas på Mac
med Focusrite Scarlett; mål för live är iPad, eventuellt Raspberry Pi senare.
Driftsäkerhet live är huvudkravet. Svara på svenska.

## Arkitektur
- `dsp/` och `engine/`: ren C++20, INGA JUCE-beroenden (portabilitet till iOS/Pi/MCU).
- `app/`: JUCE 9.0.6-skal (Standalone/AU/VST3/AUv3). Endast limkod och GUI.
- Alla effekter implementerar `gfx::Effect` (prepare/reset/process/latencySamples).
- Kedja: in-gain → EffectChain → ut-gain → SafetyLimiter (alltid sist).
- Parametrar: `gfx::Parameter` (atomic) + `SmoothedValue` på ljudtråden.

## Regler
- process()/reset(): ingen allokering, inga lås, ingen loggning, ingen I/O.
- Varje ny effekt: tester i `tests/` för tystnad, extrema parametrar (inga NaN)
  och att `process()` inte allokerar (se `NoAllocScope`).
- Ändra aldrig så att limitern kan förbikopplas.

## Kommandon
- Konfigurera: `cmake -B build -G Xcode` (eller utan -G för Makefiles/Ninja)
- Bygg: `cmake --build build --config Release -j`
- Tester: `ctest --test-dir build -C Release --output-on-failure`
- Bara kärnan (snabbt): `cmake -B build-core -DGFX_BUILD_APP=OFF && cmake --build build-core`
- Offline: `build/tools/offline-render/gfx-render in.wav ut.wav`

## Nästa steg
Se "Färdplan" i README.md. Nästa: mät latens på Mac + Scarlett, sedan distortion
med oversampling och kabinett-IR.
