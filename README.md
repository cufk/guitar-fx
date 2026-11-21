# GuitarFX

Egen gitarreffektprocessor: chorus, flanger, reverb och distortion. Utvecklas på Mac
med Focusrite Scarlett och är byggd för att flyttas till iPad (live) och eventuellt
Raspberry Pi eller egen pedal.

## Status (v0.1 – skelett)

- Portabel DSP-kärna och ljudmotor utan beroenden (`dsp/`, `engine/`)
- Passthrough: gitarr in på vald ingång → dual mono ut
- Säkerhetslimiter sist i kedjan (−1 dBFS tak, NaN/Inf-skydd, automatisk återställning)
- Effektkedja med klickfri bypass (15 ms crossfade)
- JUCE-app: Standalone + AU/VST3 (Mac), AUv3 (iOS), VST3 (Linux)
- Scen-GUI v0: in/ut-gain, nivåmätare, CPU, limiter, watchdog-varning
- Offline-verktyg `gfx-render` och tester (inkl. att ljudtråden inte allokerar minne)

Inga riktiga effekter ännu – de läggs till i `dsp/effects/` enligt färdplanen nedan.

## Struktur

```
dsp/            Ren C++20. Effect-interface, buffrar, parametrar, effekter.
engine/         Effektkedja och Engine (in-gain → kedja → ut-gain → limiter).
app/            JUCE-skal (Standalone/plugin) + GUI. Endast limkod.
tools/          gfx-render: kör motorn på WAV-filer.
tests/          Beroendefria tester.
presets/        (kommer) JSON-presets.
```

Regel: `dsp/` och `engine/` får aldrig inkludera JUCE. Det är det som gör att samma
kod kan köras på iOS, Raspberry Pi och mikrokontroller.

## Bygga på Mac

Krav: Xcode (med kommandoradsverktyg) och CMake (`brew install cmake`).
JUCE 9.0.6 hämtas automatiskt vid första konfigureringen.

```bash
# Xcode-projekt (rekommenderas för att köra/debugga appen)
cmake -B build -G Xcode
open build/GuitarFX.xcodeproj      # välj schemat "GuitarFX_Standalone" och kör

# Eller kommandoraden
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
ctest --test-dir build -C Release --output-on-failure
```

Appen hamnar i `build/app/GuitarFX_artefacts/<Config>/Standalone/GuitarFX.app`.

### Första körningen med Scarlett

1. Koppla gitarren till **ingång 1** och slå på **INST** på Scarlett.
2. Starta appen → **Options → Audio/MIDI Settings**: välj Scarlett som in och ut,
   48 kHz och buffertstorlek 64 (prova 32 när allt fungerar).
3. JUCE:s fristående app tystar ingången som standard för att undvika rundgång
   ("Audio input is muted…"). Avmarkera det i inställningarna – använd hörlurar
   eller förstärkare, inte datorns högtalare.
4. Ställ in gain på Scarlett så att IN-mätaren ligger kring gul vid hårt anslag.

## iOS (senare)

```bash
cmake -B build-ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0
open build-ios/GuitarFX.xcodeproj
```

Sätt ditt Team under Signing i Xcode. Använd ett betalt utvecklarkonto – byggen med
gratiskonto slutar starta efter 7 dagar, vilket är oacceptabelt live.

## Linux / Raspberry Pi (senare)

Samma CMake-bygge fungerar (verifierat på Ubuntu 24.04 x86_64). Kräver bl.a.
`libasound2-dev libfreetype-dev libfontconfig1-dev libx11-dev libxrandr-dev
libxinerama-dev libxcursor-dev libxext-dev libxi-dev`. För en huvudlös pedal på Pi
byggs i stället ett litet ALSA/JACK-värdprogram runt `gfx::Engine`.

## Offline-verktyget

```bash
./build/tools/offline-render/gfx-render di-gitarr.wav ut.wav --in-gain 6
./build/tools/offline-render/gfx-render --sine 220 --seconds 5 test.wav
```

Spela in några torra DI-tagningar direkt från Scarlett och använd dem som
referens när effekterna utvecklas.

## Färdplan

1. ✅ Skelett, passthrough, limiter, tester
2. Mät latens rundtur på Mac + Scarlett (loopback-kabel ut→in)
3. Distortion (waveshaper + 4x oversampling) och kabinett-IR (faltning)
4. `ModulatedDelay` → Chorus och Flanger
5. Reverb (Freeverb först, sedan FDN/plate)
6. Presets (JSON), MIDI-fotswitch, scenläge
7. iOS-bygge och live-härdning (ljudsessionsavbrott, återanslutning av ljudkort)
8. (Valfritt) Raspberry Pi / Daisy Seed

## Live-säkerhet – regler som alltid gäller

- Ljudtråden: ingen allokering, inga lås, ingen loggning, ingen fil-I/O
  (testet `Realtidssäkerhet` fångar allokeringar).
- Säkerhetslimitern sitter alltid sist och kan inte förbikopplas.
- Varje ny effekt ska ha tester för: tystnad in → tystnad ut, ingen NaN vid extrema
  parametrar, och ingen allokering i `process()`.
- Hårdvarumässig reservväg (ABY/true bypass-switch) före ljudkortet vid spelning.
