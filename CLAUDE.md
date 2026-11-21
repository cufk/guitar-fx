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
- Presets: `gfx::Rig` (engine/Rig.h) – grundljud + presets (ett i taget). Används bara från
  meddelandetråden; processorns timer kör `syncFromLive()` och MIDI-kommandon (50 Hz).
- GUI: `app/Source/ui/` – StemSplitter-formspråket (Theme: färger, kort, MiniSlider;
  Widgets: EffectRow, PresetPanel, InputPanel, HeaderBar, BottomBar).
  Ny effekt = lägg till i kedjan + `rig_.addEffect({...})` i processorn; raden och
  sparningen kommer automatiskt.

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
Se "Färdplan" i README.md. Klart: akustisk ingång, EQ med notch, chorus, flanger, reverb
(FDN, utklingning vid bypass), presets (Rig) med MIDI/tangenter, GUI i StemSplitter-stil,
distortion, kabinett-IR, latensmätare (5,60 ms rundtur, buffert 64).
Nästa: kompressor och delay; MIDI learn.
Känt: effekter före chorus bearbetar båda kanalerna fast signalen är mono (dubbel CPU) –
värt att åtgärda före Raspberry Pi. getStateInformation läser Rig och antar
meddelandetråden (gäller standalone; vissa plugin-värdar anropar från annan tråd).
