// gfx-render: kör ljudmotorn på en WAV-fil utan ljudkort.
//
//   gfx-render in.wav ut.wav [--in-gain dB] [--out-gain dB] [--block N]
//              [--drive dB] [--mode 0|1|2] [--tone 0..1] [--level dB]
//              [--cab] [--ir fil.wav] [--cab-level dB]
//              [--chorus mix] [--flanger mix] [--reverb sekunder] [--reverb-mix mix]
//   gfx-render --sine 440 --seconds 2 ut.wav     (testsignal i stället för fil)
//
// --drive slår på distortion, --cab det inbyggda kabinettet och --ir laddar en
// egen kabinett-IR (slår också på kabinettet).
//
// Spela in några DI-tagningar (torr gitarr direkt från Scarlett) en gång,
// och använd dem sedan för att lyssna på och jämföra effekter.

#include "WavFile.h"
#include "engine/Engine.h"
#include "dsp/effects/Cabinet.h"
#include "dsp/effects/Chorus.h"
#include "dsp/effects/Flanger.h"
#include "dsp/effects/Reverb.h"
#include "dsp/effects/Distortion.h"
#include <memory>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    std::vector<std::string> pos;
    float inGain = 0.0f, outGain = 0.0f, sineHz = 0.0f, seconds = 2.0f;
    float drive = -1.0f, mode = 0.0f, tone = 0.5f, level = -6.0f, cabLevel = 0.0f;
    bool useCab = false;
    float chorusMix = -1.0f, flangerMix = -1.0f, reverbSec = -1.0f, reverbMix = 0.25f;
    std::string irPath;
    int block = 64;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&] { return i + 1 < argc ? std::atof(argv[++i]) : 0.0; };
        if      (a == "--in-gain")  inGain  = static_cast<float>(next());
        else if (a == "--out-gain") outGain = static_cast<float>(next());
        else if (a == "--block")    block   = static_cast<int>(next());
        else if (a == "--sine")     sineHz  = static_cast<float>(next());
        else if (a == "--seconds")  seconds = static_cast<float>(next());
        else if (a == "--drive")    drive   = static_cast<float>(next());
        else if (a == "--mode")     mode    = static_cast<float>(next());
        else if (a == "--tone")     tone    = static_cast<float>(next());
        else if (a == "--level")    level   = static_cast<float>(next());
        else if (a == "--cab")      useCab  = true;
        else if (a == "--cab-level") cabLevel = static_cast<float>(next());
        else if (a == "--chorus")   chorusMix  = static_cast<float>(next());
        else if (a == "--flanger")  flangerMix = static_cast<float>(next());
        else if (a == "--reverb")   reverbSec  = static_cast<float>(next());
        else if (a == "--reverb-mix") reverbMix = static_cast<float>(next());
        else if (a == "--ir" && i + 1 < argc) { irPath = argv[++i]; useCab = true; }
        else pos.push_back(a);
    }

    gfx::wav::Audio audio;
    std::string err, outPath;

    if (sineHz > 0.0f && pos.size() == 1) {
        outPath = pos[0];
        audio.sampleRate = 48000;
        const int n = static_cast<int>(seconds * audio.sampleRate);
        audio.channels.assign(1, std::vector<float>(static_cast<size_t>(n)));
        for (int i = 0; i < n; ++i)
            audio.channels[0][static_cast<size_t>(i)] =
                0.5f * std::sin(2.0f * 3.14159265f * sineHz * static_cast<float>(i) / audio.sampleRate);
    } else if (pos.size() == 2) {
        outPath = pos[1];
        if (!gfx::wav::read(pos[0], audio, err)) { std::fprintf(stderr, "Fel: %s\n", err.c_str()); return 1; }
    } else {
        std::fprintf(stderr,
            "Användning:\n  gfx-render in.wav ut.wav [--in-gain dB] [--out-gain dB] [--block N]\n"
            "             [--drive dB] [--mode 0|1|2] [--tone 0..1] [--level dB]\n"
            "             [--cab] [--ir fil.wav] [--cab-level dB]\n"
            "             [--chorus mix] [--flanger mix] [--reverb sekunder] [--reverb-mix mix]\n"
            "  gfx-render --sine HZ [--seconds S] ut.wav\n");
        return 2;
    }

    size_t tailExtra = 0;
    gfx::Engine engine;
    engine.inputGain().gainDb.set(inGain);
    engine.outputGain().gainDb.set(outGain);

    if (drive >= 0.0f) {
        auto d = std::make_unique<gfx::Distortion>();
        d->driveDb.set(drive); d->mode.set(mode); d->tone.set(tone); d->levelDb.set(level);
        engine.chain().add(std::move(d));
    }
    if (useCab) {
        auto c = std::make_unique<gfx::Cabinet>();
        c->levelDb.set(cabLevel);
        if (!irPath.empty()) {
            gfx::wav::Audio ir;
            if (!gfx::wav::read(irPath, ir, err) || ir.channels.empty()) { std::fprintf(stderr, "Fel (IR): %s\n", err.c_str()); return 1; }
            if (!c->loadImpulseResponse(ir.channels[0].data(), ir.numSamples(), ir.sampleRate, irPath)) {
                std::fprintf(stderr, "Fel: ogiltig IR %s\n", irPath.c_str()); return 1;
            }
        }
        engine.chain().add(std::move(c));
    }
    if (chorusMix >= 0.0f)  { auto c = std::make_unique<gfx::Chorus>();  c->mix.set(chorusMix);  engine.chain().add(std::move(c)); }
    if (flangerMix >= 0.0f) { auto f = std::make_unique<gfx::Flanger>(); f->mix.set(flangerMix); engine.chain().add(std::move(f)); }
    if (reverbSec > 0.0f) {
        auto r = std::make_unique<gfx::Reverb>();
        r->decaySec.set(reverbSec); r->mix.set(reverbMix);
        engine.chain().add(std::move(r));
        // Låt efterklangen klinga ut i filen.
        tailExtra = static_cast<size_t>(std::min(reverbSec, 10.0f) * audio.sampleRate);
    }

    // Stereo ut, gitarren på kanal 0 (precis som Scarlett ingång 1).
    std::vector<float> l(audio.channels[0]);
    l.resize(l.size() + tailExtra, 0.0f);
    const int n = static_cast<int>(l.size());
    std::vector<float> r(static_cast<size_t>(n), 0.0f);
    float* ptrs[2] = { l.data(), r.data() };
    engine.prepare({ static_cast<double>(audio.sampleRate), block, 2 });

    const auto t0 = std::chrono::steady_clock::now();
    for (int start = 0; start < n; start += block) {
        float* p[2] = { ptrs[0] + start, ptrs[1] + start };
        engine.process({ p, 2, std::min(block, n - start) });
    }
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    gfx::wav::Audio out;
    out.sampleRate = audio.sampleRate;
    out.channels = { std::move(l), std::move(r) };
    if (!gfx::wav::write(outPath, out, err)) { std::fprintf(stderr, "Fel: %s\n", err.c_str()); return 1; }

    const double audioSecs = static_cast<double>(n) / audio.sampleRate;
    std::printf("Klart: %s (%.2f s ljud på %.3f s = %.0fx realtid, block %d, latens %d smp, återställningar %u)\n",
                outPath.c_str(), audioSecs, secs, secs > 0 ? audioSecs / secs : 0.0, block,
                engine.latencySamples(), engine.recoveryCount());
    return 0;
}
