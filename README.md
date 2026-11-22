# Stagebox

(Tidigare *GuitarFX*. Repot på GitHub heter fortfarande `guitar-fx`.)

Egen gitarreffektprocessor: chorus, flanger, reverb och distortion. Utvecklas på Mac
med Focusrite Scarlett och är byggd för att flyttas till iPad (live) och eventuellt
Raspberry Pi eller egen pedal.

## Status (v0.9 – Stagebox: stämfält längst ned, stämningar, låtanalys)

- Portabel DSP-kärna och ljudmotor utan beroenden (`dsp/`, `engine/`)
- **Ingångssteg för akustisk gitarr:** Ingång 1, Ingång 2 eller **Blanda 1 + 2**
  (t.ex. mikrofon + inbyggd mikrofon), balans, polaritetsvändning per ingång och
  tidsjustering med **Auto-justera** (mäter fördröjning och polaritet medan du spelar).
  Mono ut på alla utgångar (dual mono).
- Säkerhetslimiter sist i kedjan (−1 dBFS tak, NaN/Inf-skydd, automatisk återställning)
- Effektkedja med klickfri bypass (15 ms crossfade)
- **Grundljud + presets:** grundljudet (t.ex. EQ och lite reverb) ligger alltid på. Presets
  är en eller flera effekter med egna inställningar som aktiveras ovanpå – ett i taget,
  tryck igen för att gå tillbaka till grundljudet. Styrs med knapparna, tangenter eller
  MIDI-fotswitch (även Bluetooth). Reverbsvansen klingar ut naturligt vid byte.
- **EQ:** lågskärning, bas, mitt (valbar frekvens), diskant och smal notch mot rundgång.
- **Låtanalys → preset:** välj en låt (WAV/AIFF/FLAC/MP3/M4A). Appen analyserar klang,
  rum, bredd, dynamik och tempo lokalt och föreslår ett preset med låtens namn.
- **Stämapparat alltid synlig:** ett lågt fält längst ned visar ton, skala −50…+50 cent,
  avvikelse och strängarna i vald stämning (Standard, Drop D, halvton/helton ner, Drop C,
  DADGAD, Open G/D/E/C eller Kromatisk). **Mute** (eller T / MIDI CC 105) stänger av ljudet
  ut medan du stämmer – då blir fältet svart. Referens A4 430–446 Hz. Noggrannhet < 1 cent.
- **Sustain:** kompressor för längre ton – sustain, anslag, släpp, nivå och mix
  (parallellkompression som behåller anslaget). Visar hur mycket den komprimerar.
- **Widener:** bredare stereo med mitt/sida-teknik – exakt monokompatibel (L+R = originalet).
- **Utgång Stereo / Mono (summa):** mono om bara en kanal går till mixern.
- **Monokromt GUI med runda rattar:** varm off-white (eller mörkt), stora rubriker,
  ring av streck som visar utnivån runt aktivt presetnummer, effekterna som rutor och
  presets som piller. Eget appfönster utan JUCE:s "Options"-rad. Utseende: följ
  systemet, ljust eller mörkt (⋯ → Utseende). Typsnitt: Inter (inbäddat, OFL-licens).
- **Chorus** (hastighet, djup, mix) och **Flanger** (hastighet, djup, återkoppling, mix),
  båda i stereo: vänster och höger sveper i olika fas.
- **Reverb** (FDN med 8 linjer): efterklang 0,3–10 s, storlek, predelay, dämpning,
  lågskärning på efterklangen och mix. Efterklangstiden stämmer med inställningen
  (testat), och slingan är stabil för alla inställningar.
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
  core/         Biquad, DelayLine/Lfo, Oversampler4x, FFT, Convolver, PitchDetector,
                Parameter, DspUtils
  effects/      Gain, SafetyLimiter, Eq, Sustain, Distortion, Cabinet, Chorus, Flanger,
                Widener, Reverb
engine/         InputMixer, effektkedja, Engine (ingång → in-gain → kedja → ut-gain →
                limiter), LatencyProbe, TunerTap, Rig (grundljud + presets), SongAnalyzer.
app/Source/ui/  Tema (monokromt, Inter), ikoner och GUI-komponenter.
app/Resources/  Typsnitt (Inter, SIL Open Font License).
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
open build/Stagebox.xcodeproj      # välj schemat "Stagebox_Standalone" och kör

# Eller kommandoraden
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
ctest --test-dir build -C Release --output-on-failure
```

Appen hamnar i `build/app/Stagebox_artefacts/<Config>/Standalone/Stagebox.app`.

### Första körningen med Scarlett

Utgångarna på baksidan heter **Left** och **Right** (= utgång 1 och 2). Stagebox skickar
samma signal på båda; använd **Left** till mixer/PA. Hörlurar i Scarlettens hörlursuttag.
Ingång och utgång måste vara **samma ljudenhet** (Scarlett), annars ökar latensen kraftigt.

1. Koppla gitarren till **ingång 1** och slå på **INST** på Scarlett
   (eller se avsnittet om akustisk gitarr nedan för mikrofon + inbyggd).
2. Starta appen → **⋯ → Ljudinställningar…**: välj Scarlett 2i2 som både utgång och
   ingång, kryssa i kanal 1 + 2 för båda, 48 kHz och buffertstorlek 64.
3. Använd hörlurar eller förstärkare, inte datorns högtalare (rundgång).
4. Ställ in gain på Scarlett så att IN-mätaren ligger kring gul vid hårt anslag.

## iOS (senare)

```bash
cmake -B build-ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0
open build-ios/Stagebox.xcodeproj
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

Öppna **⋯ → Ingång och utgång…** och välj **Källa**:
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

### Effekter för akustisk gitarr – startinställningar

Kedjan är: distortion → kabinett → **chorus → flanger → reverb**. Allt är av vid start.

- **Reverb** – det viktigaste. Börja med efterklang 1,5–2,5 s, mix 0,2–0,3, predelay
  10–25 ms (håller anslaget tydligt). Höj **Lågskärning** (150–250 Hz) om det blir grötigt
  i PA:t, sänk **Dämpning** (4000–6000 Hz) för en varmare, mindre "glasig" svans.
- **Chorus** – låg hastighet (0,3–0,8 Hz), djup 0,3–0,5, mix 0,3. Ger bredd utan att
  gitarren låter ostämd.
- **Flanger** – mer en specialeffekt på akustisk gitarr; låg hastighet och mix 0,3
  för ett mjukt svep, hög återkoppling för en tydlig "jet"-effekt.

Stereo: chorus, flanger och reverb ger olika signal i Left och Right. Med bara Left till
mixern hörs effekterna fortfarande fullt ut (i mono). Koppla Right till en andra kanal
för stereo i PA:t.

## Grundljud och presets

**Grundljudet** är det som hörs när inget preset är aktivt. Välj pillret **0 Grundljud**
längst ned och ställ in EQ och de effekter som alltid ska vara på (t.ex. ett diskret rum).

**Gränssnittet:**
- Rutorna uppe till höger är effekterna (fylld prick = på). Tryck på en för att visa dess
  rattar nedanför. Rattarna följer samma fyra kolumner som effektrutorna (två rattar tar
  två kolumner). Dra en ratt uppåt/nedåt; dubbelklick återställer standardvärdet.
- Reglaget till höger om effektens namn slår på/av den.
- Vänster spalt visar aktivt preset (stort nummer och namn), utnivån som en ring av streck
  och ← / → för att bläddra.
- Alla inställningar finns under **⋯** uppe till höger: ingång och utgång, ljudinställningar,
  latensmätning, export/import av presets och utseende.
- Ikonen med ljudvågen bredvid **+** längst ned öppnar låtanalysen.

**Ett preset** är en eller flera effekter med egna inställningar:
1. Tryck **+** längst ned till höger och ge presetet ett namn.
2. Välj en effekt och tryck **I preset**. Effekten slås på och dina ändringar sparas nu i
   presetet (rutan visar PRESET).
3. Effekter utan I preset följer grundljudet. Ändrar du dem medan presetet är aktivt
   ändras grundljudet.

**Live:** tryck på ett presetpiller för att aktivera det, tryck igen (eller på Grundljud)
för att gå tillbaka. Byten glider mjukt och reverbsvansen klingar ut. Högerklick/långt
tryck på ett piller ger meny för att byta namn, duplicera, flytta och ta bort. Allt sparas
automatiskt; **⋯ → Exportera/Importera presets** sparar en JSON-fil (backup, flytta till iPad).

Appen startar med exempel – *Stor hall*, *Chorus* och *Flanger-svep* – att ändra eller ta bort.

### Styra med fotswitch eller tangentbord

| Styrning | Funktion |
|---|---|
| Tangent **1–9** | Preset 1–9 (samma igen = grundljud) |
| **0** eller **Esc** | Grundljud |
| **←** / **→** (även Page Up/Down) | Föregående / nästa |
| MIDI **Program Change 0–8** | Preset 1–9 |
| MIDI **CC 102 / 103 / 104** (värde ≥ 64) | Föregående / nästa / grundljud |
| Tangent **T** eller MIDI **CC 105** | Mute (stämning) på/av |

MIDI-ingångar (även Bluetooth-MIDI-pedaler som parats i macOS *Ljud-MIDI-inställningar*)
öppnas automatiskt. Ljudkort och MIDI väljs under **⋯ → Ljudinställningar…**. Pedaler som i stället
skickar tangenttryck (sidvändare för noter) fungerar direkt med ←/→ eller Page Up/Down.
Senast mottagna MIDI visas längst ned till höger.

## Låtanalys

Tryck på ikonen med ljudvågen bredvid **+** längst ned (eller ⋯ → Analysera låt…) och välj en ljudfil. Analysen tar
någon sekund och sker helt lokalt – inget laddas upp och ingen modell laddas ner.

| Område | Vad som mäts | Blir i presetet |
|---|---|---|
| Klang | Medelspektrum jämfört med en typisk mix (−4,5 dB/oktav) | EQ: lågskärning, bas, mitt (frekvens), diskant |
| Rum | Hur nivån klingar ut efter anslag (helst i pauser) och hur mycket som ligger kvar mellan tonerna | Reverb: efterklang, mix, predelay, storlek |
| Bredd | Sida/mitt-förhållande | Widener (om låten är tydligt stereo) |
| Dynamik | Spridning i korttidsnivå (p95 − p10) | Sustain (om låten är komprimerad) |
| Tempo | Anslagsrytm, 60–180 BPM | Visas (används för delay senare) |

**Skapa preset** lägger till ett preset med låtens namn och aktiverar det. Analysen lyssnar på
hela mixen (sång, trummor), så se förslaget som en startpunkt och justera med öronen. Rummet
mäts bäst i låtar med pauser eller ett tydligt slut; i tätt spelade låtar underskattas långa
reverber något.

## Mäta latens (färdplanens steg 2)

1. Koppla bort förstärkare/hörlurar. Slå **av** INST på ingången och vrid ner gain.
2. Koppla en kabel från Scarletts **Line Out Left** (baksidan) till **ingång 1**.
3. Välj **⋯ → Mät latens…** och sedan **Starta**. Resultatet visas i vänster spalt. Appen skickar 5 klick på −12 dBFS och
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

Effekter: `--chorus 0.4` (mix), `--flanger 0.5` (mix), `--reverb 2` (efterklang i s,
med `--reverb-mix 0.25`). Filen förlängs så att efterklangen hinner klinga ut.

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
5. ✅ Chorus, flanger och reverb (FDN)
6. ✅ EQ med notch, grundljud + presets, MIDI/tangenter
7. ✅ Sustain (kompressor), widener, mono-utgång, monokromt GUI med runda rattar
8. ✅ Stämapparat
9. ✅ Låtanalys → förslag på preset (signalanalys, lokalt)
10. Delay
11. Kroppssimulering för inbyggd mikrofon (IR via befintlig faltning)
12. MIDI learn (valfria meddelanden per preset), scenläge med extra stora knappar
13. iOS-bygge och live-härdning (ljudsessionsavbrott, återanslutning av ljudkort)
14. (Valfritt) Raspberry Pi / Daisy Seed

## Live-säkerhet – regler som alltid gäller

- Ljudtråden: ingen allokering, inga lås, ingen loggning, ingen fil-I/O
  (testet `Realtidssäkerhet` fångar allokeringar).
- Säkerhetslimitern sitter alltid sist och kan inte förbikopplas.
- Varje ny effekt ska ha tester för: tystnad in → tystnad ut, ingen NaN vid extrema
  parametrar, och ingen allokering i `process()`.
- Strängar med å/ä/ö i JUCE-koden måste skrivas som `juce::String::fromUTF8("...")`
  (JUCE tolkar annars `const char*` som ASCII).
- Hårdvarumässig reservväg (ABY/true bypass-switch) före ljudkortet vid spelning.
