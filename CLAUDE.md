# CLAUDE.md – GuitarFX

Egen gitarreffektprocessor. **Primärt fokus nu: akustisk gitarr** (mikrofon och/eller
inbyggd mikrofon, Scarlett 2i2 in 1 + 2) → mixer/PA. Distortion/kabinett finns för
elgitarr men är avstängda som standard. Utvecklas på Mac; mål för live är iPad,
eventuellt Raspberry Pi senare.
Driftsäkerhet live är huvudkravet. Svara på svenska.

## Arkitektur
- `dsp/` och `engine/`: ren C++20, INGA JUCE-beroenden (portabilitet till iOS/Pi/MCU).
- `app/`: JUCE 9.0.6-skal (Standalone/AU/VST3/AUv3). Endast limkod och GUI.
- Alla effekter implementerar `gfx::Effect` (prepare/reset/process/latencySamples).
- Kedja: InputMixer (in 1/2/blanda → mono) → in-gain → EffectChain → ut-gain → SafetyLimiter (alltid sist).
- Parametrar: `gfx::Parameter` (atomic) + `SmoothedValue` på ljudtråden.

## Regler
- process()/reset(): ingen allokering, inga lås, ingen loggning, ingen I/O.
- Varje ny effekt: tester i `tests/` för tystnad, extrema parametrar (inga NaN)
  och att `process()` inte allokerar (se `NoAllocScope`).
- Ändra aldrig så att limitern kan förbikopplas.
- JUCE-strängar med å/ä/ö: `juce::String::fromUTF8("...")` (annars tolkas de som ASCII).
- Byte av tunga resurser (IR, senare presets) under spelning: bygg utanför ljudtråden,
  lämna över via atomisk pekare, radera på icke-ljudtråd (mönster i `Cabinet`).

## Kommandon
- Konfigurera: `cmake -B build -G Xcode` (eller utan -G för Makefiles/Ninja)
- Bygg: `cmake --build build --config Release -j`
- Tester: `ctest --test-dir build -C Release --output-on-failure`
- Bara kärnan (snabbt): `cmake -B build-core -DGFX_BUILD_APP=OFF && cmake --build build-core`
- Offline: `build/tools/offline-render/gfx-render in.wav ut.wav`

## Nästa steg
Se "Färdplan" i README.md. Klart: akustisk ingång (blanda + auto-justering),
chorus, flanger, reverb (FDN), distortion, kabinett-IR, latensmätare (5,60 ms
rundtur, buffert 64). Nästa: akustisk EQ + notch mot rundgång, sedan kompressor och delay.
GUI: nya effekter läggs till med `addFxPanel()` i editorn och `registerParam()` /
`slots_` i processorn (sparas automatiskt i state).
Möjlig förbättring: reverbsvansen klipps vid bypass (15 ms fade) – "spillover" vore bättre live.
Känd förbättring: effekter före chorus (distortion, kabinett, kommande EQ) bearbetar
båda kanalerna fast signalen fortfarande är mono (dubbel CPU) – värt att åtgärda före
Raspberry Pi. Chorus/flanger/reverb gör stereo och behöver båda.
