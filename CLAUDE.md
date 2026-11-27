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
  EffectChain: EQ → Kropp → Sustain → Distortion → Kabinett → Rundgångsskydd → Dubblare →
  Chorus → Flanger → Tremolo → Widener → Delay → Reverb → Volym. Rundgångsskyddet ska ligga
  efter allt som höjer nivån och före modulation (som smetar ut topparna det letar efter).
  MIDI: PC 0–8 preset, CC 102/103/104 föreg./nästa/grundljud, 105 mute, 106 boost, 107 tap.
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
  Effektrutorna: 14 effekter i sju kolumner (`kControlColumns`, samma som reglagen) i
  signalordning (sorterade på kedjeplats); kort namn i rutan, `EffectSpec::title` i panelen.
  Skärmsläckare (`ui/ScreenSaver`): rullgardin i sidhuvudet (Ingen/Strömmar/Klot/Terräng/Form,
  Starta efter 1/5/15 min, Visa nu), sparas i processorns state. Punkter ackumuleras i en
  intensitetsbuffert → bild (alltid mörk, ingen text), styrs av TunerTap-avtappningen.
  Aktivitet = global muslyssnare (bara verkliga rörelser > 3 px – JUCE skickar konstgjorda),
  tangent, MIDI; första klicket väcker bara. 600 ms startperiod efter start.
  Boost-knappen i sidhuvudet (global, `Volume::setBoost`), Tap i delayns panel,
  Nollställ + `FilterView` för rundgångsskyddet.
  Reglagen: en rad med sju lika breda platser (`kControlColumns`) som fyller höjden – samma
  storlek oavsett effekt. InputPanel använder fortfarande runda rattar (`KnobTile(spec, false)`).
  Val (`ParamSpec::choices`, t.ex. kroppstyp, distortion-typ, delayns indelning) visas som
  rullgardin (`Pill::chevron` + PopupMenu) i rubrikraden närmast av/på-reglaget – inte som reglageruta.
  Ingen "I presetet"-knapp (användaren tog bort den): att en effekt ingår i presetet syns som
  I PRESET på rutan och i statusraden; högerklick/långt tryck på rutan = ta med / följ grundljudet.
- Stämapparat: lågt fält längst ned (`TunerStrip`), stämningar i `ui::tunings()`, valet och
  A4 sparas i processorns state. `engine/TunerTap.h` (ljudtråden kopierar torr ingång till
  ringbuffert; "Mute" gäller bara medan GUI:t är öppet) + `dsp/core/PitchDetector.h` (YIN).
- Låtanalys: `engine/SongAnalyzer` (ren signalanalys av hela mixen: balans per område, rum,
  bredd, dynamik, tempo) körs i egen tråd från `AnalysisView`. `engine/GuitarFit` översätter
  till gitarrens plats i bandet – gitarren ska KOMPLETTERA mixen, inte härma den (användarens
  krav): mycket bas → högre lågskärning och basavsänkning, full lågmitt → sänkning, ficka i
  2–5 kHz → liten höjning, mörk mix → diskant, rum torrare än låten, ingen widener.
  `GuitarFXProcessor::createPresetFromAnalysis(GuitarFit)` gör presetet.
  Rum: stereo via L/R-koherens, mono via sen avklingning (tvådelad anpassning), annars
  standardval. Mät aldrig rum som "median av avklingning" i tät musik – det ger alltid max.
  Kalibrerat mot CC-låtar från Internet Archive (testas lokalt, checkas inte in).
  Användaren valde signalanalys (inte Demucs/AI-separering) – Demucs kan bli ett Mac-tillval.
  Ny effekt = lägg till i kedjan + `rig_.addEffect({...})` i processorn; ruta, reglage och
  sparning kommer automatiskt.
- Varumärke: `app/Resources/BRAND.md` (färger, filer). Appikon via `ICON_BIG` i
  `app/CMakeLists.txt` (macOS: `app-icon/macos/icon-macos-light-1024.png` med Apples rutnät,
  iOS: `app-icon/ios/icon-ios-light-1024.png`). Logotypen i sidhuvudet ritas från
  `Resources/logo/app/stagebox-logo-{light,dark}.svg` (märkets transform förberäknad – JUCE:s
  SVG-läsare klarar inte kedjade transformer) och placeras efter ritrutan 1200 × 314, inte
  efter `getDrawableBounds()` (som missar linjernas tjocklek).
- iPad: samma kod (`#if JUCE_IOS` där det behövs). Helskärm liggande (CMake: TARGETED_DEVICE_FAMILY 2,
  IPAD_SCREEN_ORIENTATIONS landskap, STATUS_BAR_HIDDEN), ingen fönstergräns (layouten ska klara
  iPad mini 1133 × 744). Filval: använd alltid `ui::localCopyOf(fc, dir)` – på iPad ger Filer-appen
  bara en tillfällig URL. Sparar i `suspended()`. CI-jobbet `ipad` bygger för riktig iPad och tar
  skärmdumpar i simulatorn (`scripts/ipad-screenshots.sh`).
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
Stämapparat och låtanalys klara. Rundgångsskydd, delay + tap tempo, kropp, dubblare/oktav,
tremolo och volym/boost klara (nov 2026). Nästa: looper; MIDI learn; scenläge för iPad.
Känt: effekter före chorus bearbetar båda kanalerna fast signalen är mono (dubbel CPU) –
värt att åtgärda före Raspberry Pi. getStateInformation läser Rig och antar
meddelandetråden (gäller standalone; vissa plugin-värdar anropar från annan tråd).
