// gfx-render: kör ljudmotorn på en WAV-fil utan ljudkort.
//
//   gfx-render in.wav ut.wav [--in-gain dB] [--out-gain dB] [--block N]
//   gfx-render --sine 440 --seconds 2 ut.wav     (testsignal i stället för fil)
//
// Spela in några DI-tagningar (torr gitarr direkt från Scarlett) en gång,
// och använd dem sedan för att lyssna på och jämföra effekter.

#include "WavFile.h"
#include "engine/Engine.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    std::vector<std::string> pos;
    float inGain = 0.0f, outGain = 0.0f, sineHz = 0.0f, seconds = 2.0f;
    int block = 64;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&] { return i + 1 < argc ? std::atof(argv[++i]) : 0.0; };
        if      (a == "--in-gain")  inGain  = static_cast<float>(next());
        else if (a == "--out-gain") outGain = static_cast<float>(next());
        else if (a == "--block")    block   = static_cast<int>(next());
        else if (a == "--sine")     sineHz  = static_cast<float>(next());
        else if (a == "--seconds")  seconds = static_cast<float>(next());
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
            "  gfx-render --sine HZ [--seconds S] ut.wav\n");
        return 2;
    }

    // Stereo ut, gitarren på kanal 0 (precis som Scarlett ingång 1).
    const int n = audio.numSamples();
    std::vector<float> l(audio.channels[0]), r(static_cast<size_t>(n), 0.0f);
    float* ptrs[2] = { l.data(), r.data() };

    gfx::Engine engine;
    engine.inputGain().gainDb.set(inGain);
    engine.outputGain().gainDb.set(outGain);
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
    std::printf("Klart: %s (%.2f s ljud på %.3f s = %.0fx realtid, block %d, återställningar %u)\n",
                outPath.c_str(), audioSecs, secs, secs > 0 ? audioSecs / secs : 0.0, block,
                engine.recoveryCount());
    return 0;
}
