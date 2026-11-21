# GuitarFX

Egen gitarreffektprocessor: chorus, flanger, reverb och distortion. Utvecklas på Mac
med Focusrite Scarlett och är byggd för att flyttas till iPad (live) och eventuellt
Raspberry Pi eller egen pedal.

## Status (v0.3 – akustisk ingång, distortion, kabinett, latensmätning)

- Portabel DSP-kärna och ljudmotor utan beroenden (`dsp/`, `engine/`)
- **Ingångssteg för akustisk gitarr:** Ingång 1, Ingång 2 eller **Blanda 1 + 2**
  (t.ex. mikrofon + inbyggd mikrofon), balans, polaritetsvändning per ingång och
  tidsjustering med **Auto-justera** (mäter fördröjning och polaritet medan du spelar).
  Mono ut på alla utgångar (dual mono).
- Säkerhetslimiter sist i kedjan (−1 dBFS tak, NaN/Inf-skydd, automatisk återställning)
- Effektkedja med klickfri bypass (15 ms crossfade)
- **Distortion** med 4x oversampling (halvbands-FIR, ~0,58 ms latens vid 48 kHz):
  lägena Overdrive / Distortion / Fuzz, rattar Drive, Tight (lågskärning före
  klippning), Ton och Nivå. Uppmätt aliasing < −100 dB (7 kHz-ton, 24 dB drive).
- **Kabinett** med IR-faltning utan latens (direkt huvud + partitionerad FFT-svans).
  Inbyggt syntetiskt 1x12-svar, eller ladda egen IR (WAV/AIFF, valfri
  samplingsfrekvens – samplas om automatiskt, normaliseras, max 0,5 s).
  Byte av IR under spelning är låsfritt.
- **Latensmätning** i appen: loopback-kabel ut → in, knappen "Mät latens".
- JUCE-app: Standalone + AU/VST3 (Mac), AUv3 (iOS), VST3 (Linux)
- Scen-GUI v1: nivåer, distortion, kabinett, mätare, CPU, limiter, watchdog-varning
- Offline-verktyg `gfx-render` (med effekterna) och 19 tester (inkl. att ljudtråden
  inte allokerar minne och att IR-byte från annan tråd är säkert)

## Struktur

```
dsp/            Ren C++20. Effect-interface, buffrar, parametrar, effekter.
  core/         Biquad, Oversampler4x, FFT, Convolver, Parameter, DspUtils
  effects/      Gain, SafetyLimiter, Distortion, Cabinet
engine/         InputMixer, effektkedja, Engine (ingång → in-gain → kedja → ut-gain →
                limiter), LatencyProbe.
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

Utgångarna på baksidan heter **Left** och **Right** (= utgång 1 och 2). GuitarFX skickar
samma signal på båda; använd **Left** till mixer/PA. Hörlurar i Scarlettens hörlursuttag.
Ingång och utgång måste vara **samma ljudenhet** (Scarlett), annars ökar latensen kraftigt.

1. Koppla gitarren till **ingång 1** och slå på **INST** på Scarlett
   (eller se avsnittet om akustisk gitarr nedan för mikrofon + inbyggd).
2. Starta appen → **Options → Audio/MIDI Settings**: välj Scarlett 2i2 som både
   Output och Input, kryssa i Output 1 + 2 och Input 1 + 2, 48 kHz och buffertstorlek 64.
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

## Akustisk gitarr: mikrofon, inbyggd eller båda (Scarlett 2i2)

| | Ingång | Scarlett |
|---|---|---|
| Mikrofon mot gitarren | 1 (XLR) | INST av, 48V på om kondensatormikrofon |
| Gitarrens inbyggda mikrofon | 2 (teleplugg) | INST på (passiv mikrofon) |

Välj **Källa** i panelen INGÅNG:
- **Ingång 1** eller **Ingång 2** – bara en av dem.
- **Blanda 1 + 2** – båda. Den inbyggda ger stabilitet mot rundgång, mikrofonen ger luft.
  1. Ställ gain på Scarletten så att IN 1 och IN 2 ligger ungefär lika (gult vid hårt anslag).
  2. Tryck **Auto-justera** och spela i ungefär en sekund (gärna ackord). Appen mäter hur
     mycket senare mikrofonen hör gitarren och om polariteten är omvänd, och justerar.
  3. Ställ **Balans** efter smak (0 = bara ingång 1, 1 = bara ingång 2).

Utan justering släcker mikrofonen och den inbyggda delvis ut varandra och det låter tunt.
Flytta du mikrofonen: kör Auto-justera igen. **Ø**-knapparna vänder polariteten manuellt –
prova dem också om det börjar tjuta, ibland flyttar det rundgången.

Mikrofonplacering: riktad mot där halsen möter kroppen (12:e–14:e bandet), 15–30 cm bort.
Mot ljudhålet blir det mycket bas. Distortion och kabinett ska vara **av** för akustisk gitarr.

## Mäta latens (färdplanens steg 2)

1. Koppla bort förstärkare/hörlurar. Slå **av** INST på ingången och vrid ner gain.
2. Koppla en kabel från Scarletts **Line Out Left** (baksidan) till **ingång 1**.
3. Klicka **Mät latens** och sedan **Starta**. Appen skickar 5 klick på −12 dBFS och
   visar rundturslatensen (DAC + ADC + drivrutin + buffertar) i ms och samples.
   **Uppmätt (Mac + Scarlett 2i2, 48 kHz, buffert 64): 5,60 ms rundtur (269 samples).**
   Mycket bra för live – motsvarar att stå knappt 2 m från en förstärkare.
4. Prova olika buffertstorlekar (Options → Audio/MIDI Settings) och anteckna resultaten.
   Effektlatensen (oversampling, för närvarande 28 samples) kommer ovanpå när
   distortion är på och visas i statusraden.

"Ingen signal" betyder att klicket inte kom tillbaka (kabel, fel ingång, för låg gain).
Stäng av Scarletts **Direct Monitor** (och Air) före mätningen. Med loopback-kabeln
skulle Direct Monitor annars skicka ingången rakt tillbaka till utgången – en
återkopplingsslinga som kan tjuta. Monitor-ratten styr nivån på Line Out Left/Right;
ställ den på ungefär halvvägs.

## Offline-verktyget

```bash
./build/tools/offline-render/gfx-render di-gitarr.wav ut.wav --in-gain 6
./build/tools/offline-render/gfx-render di-gitarr.wav ut.wav --drive 24 --mode 0 --tone 0.6 --cab
./build/tools/offline-render/gfx-render di-gitarr.wav ut.wav --drive 30 --mode 1 --ir min-ir.wav
./build/tools/offline-render/gfx-render --sine 220 --seconds 5 test.wav
```

`--drive` slår på distortion (`--mode` 0 Overdrive, 1 Distortion, 2 Fuzz), `--cab` det
inbyggda kabinettet och `--ir` laddar en egen IR.

Spela in några torra DI-tagningar direkt från Scarlett och använd dem som
referens när effekterna utvecklas.

## Färdplan

1. ✅ Skelett, passthrough, limiter, tester
2. ✅ Mät latens rundtur på Mac + Scarlett 2i2: 5,60 ms vid 48 kHz, buffert 64
3. ✅ Distortion (waveshaper + 4x oversampling) och kabinett-IR (faltning)
   - Kvar: lyssningstest med riktiga DI-tagningar och finjustering av klangen
4. ✅ Akustisk ingång: mikrofon / inbyggd / blanda med auto-justering
5. Akustisk EQ: lågskärning, låg/mitt/diskant, notch mot rundgång
6. Reverb (Freeverb först, sedan FDN/plate)
7. Kompressor, sedan `ModulatedDelay` → Chorus, Flanger och delay
8. Kroppssimulering för inbyggd mikrofon (IR via befintlig faltning)
9. Presets (JSON), MIDI-fotswitch, scenläge
10. iOS-bygge och live-härdning (ljudsessionsavbrott, återanslutning av ljudkort)
11. (Valfritt) Raspberry Pi / Daisy Seed

## Live-säkerhet – regler som alltid gäller

- Ljudtråden: ingen allokering, inga lås, ingen loggning, ingen fil-I/O
  (testet `Realtidssäkerhet` fångar allokeringar).
- Säkerhetslimitern sitter alltid sist och kan inte förbikopplas.
- Varje ny effekt ska ha tester för: tystnad in → tystnad ut, ingen NaN vid extrema
  parametrar, och ingen allokering i `process()`.
- Strängar med å/ä/ö i JUCE-koden måste skrivas som `juce::String::fromUTF8("...")`
  (JUCE tolkar annars `const char*` som ASCII).
- Hårdvarumässig reservväg (ABY/true bypass-switch) före ljudkortet vid spelning.
