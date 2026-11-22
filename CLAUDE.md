# CLAUDE.md – Stagebox

**Stagebox** (tidigare GuitarFX; repot heter `guitar-fx`, kodens klasser heter fortfarande
`GuitarFXProcessor` m.fl. och namnrymden `gfx`). Egen gitarreffektprocessor. **Primärt fokus nu: akustisk gitarr** (mikrofon och/eller
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
  Ändringar sparas i det som är aktivt (en ändrad effekt tas automatiskt med i aktivt
  preset). Klick på rad i setlistan = aktivera; tangent/MIDI samma igen = grundljud.
  Setlistan (`SetlistPanel`, vänster) = presetordningen i Rig; dra för att ordna om,
  högerklick/långt tryck = meny; Föregående/Nästa (och MIDI CC 102/103, ←/→) stegar.
  Fristående appen autosparar state var 3:e sekund när något ändrats.
- GUI: `app/Source/ui/` – monokromt formspråk (användarens inspiration: varm off-white,
  beige rutor, svart = på/aktiv, lodräta reglage med svart runt grepp; mörkt läge inverterar).
  Godkänd design (nov 2026): ett rutnät, rubriker 22 px på samma linje (`drawSectionHeading`),
  knappar 44 px (små 34 px), mått i `ui::size`. Theme (palett, Inter-typsnitt, LookAndFeel inkl.
  lodrätt reglage),
  Icons (vektorikoner per effekt), Widgets (HeaderBar, InputMeter, SetlistPanel, EffectGrid,
  EffectPanel, KnobTile, InputPanel, TunerStrip, AnalysisView). InputMeter i sidhuvudet visar
  bara de ingångar som används (IN 1 / IN 2 enligt ingångsläget). Status (CPU, limiter,
  latens, MIDI) ligger i ⋯-menyn. Ingen färg utom rött för varningar.
  Effektrutorna: fyra kolumner (`kGridColumns`) i signalordning (sorterade på kedjeplats).
  Reglagen: en rad med sju lika breda platser (`kControlColumns`) som fyller höjden – samma
  storlek oavsett effekt. InputPanel använder fortfarande runda rattar (`KnobTile(spec, false)`).
- Stämapparat: lågt fält längst ned (`TunerStrip`), stämningar i `ui::tunings()`, valet och
  A4 sparas i processorns state. `engine/TunerTap.h` (ljudtråden kopierar torr ingång till
  ringbuffert; "Mute" gäller bara medan GUI:t är öppet) + `dsp/core/PitchDetector.h` (YIN).
- Låtanalys: `engine/SongAnalyzer` (ren signalanalys: klang, rum, bredd, dynamik, tempo) körs
  i egen tråd från `AnalysisView`; `GuitarFXProcessor::createPresetFromAnalysis` gör preset.
  Rum: stereo via L/R-koherens, mono via sen avklingning (tvådelad anpassning), annars
  standardval. Mät aldrig rum som "median av avklingning" i tät musik – det ger alltid max.
  Kalibrerat mot CC-låtar från Internet Archive (testas lokalt, checkas inte in).
  Användaren valde signalanalys (inte Demucs/AI-separering) – Demucs kan bli ett Mac-tillval.
  Ny effekt = lägg till i kedjan + `rig_.addEffect({...})` i processorn; ruta, reglage och
  sparning kommer automatiskt.
- Fristående app: eget fönster i `app/Source/StandaloneApp.cpp`
  (JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP) – inbyggd titelrad, ljudinställningar via ⋯-menyn.
  Inställningar i `Stagebox.settings` (kopieras från `GuitarFX.settings` första gången).

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
- Appen: `build/app/Stagebox_artefacts/Release/Standalone/Stagebox.app`
- Paketera (Mac): `./scripts/build-mac.sh` → `dist/Stagebox.app` + `dist/Stagebox-<version>.dmg`
- CI: `.github/workflows/build.yml` – tester på Linux; .dmg på macOS vid push till main

## Nästa steg
Se "Färdplan" i README.md. Klart: akustisk ingång, EQ med notch, sustain (kompressor),
chorus, flanger, widener, reverb (FDN, utklingning vid bypass), mono-utgång, presets (Rig)
med MIDI/tangenter, monokromt GUI med lodräta reglage, eget appfönster, distortion,
kabinett-IR, latensmätare (5,60 ms rundtur, buffert 64).
Stämapparat och låtanalys klara. Nästa: delay (kan använda låtens BPM); MIDI learn;
scenläge för iPad.
Känt: effekter före chorus bearbetar båda kanalerna fast signalen är mono (dubbel CPU) –
värt att åtgärda före Raspberry Pi. getStateInformation läser Rig och antar
meddelandetråden (gäller standalone; vissa plugin-värdar anropar från annan tråd).
