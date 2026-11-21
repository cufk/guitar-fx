#pragma once
// Gemensamt interface för alla effekter.
// Regler (gäller ALLA implementationer):
//  - prepare() får allokera minne. Anropas INTE från ljudtråden.
//  - process() och reset() körs på ljudtråden: ingen allokering, inga lås,
//    ingen loggning, ingen fil-I/O.
//  - Parametrar sätts från valfri tråd via Parameter (atomic) och glider
//    mjukt på ljudtråden.

#include "core/AudioBuffer.h"
#include <string_view>

namespace gfx {

struct ProcessSpec {
    double sampleRate   = 48000.0;
    int    maxBlockSize = 512;
    int    numChannels  = 2;
};

class Effect {
public:
    virtual ~Effect() = default;

    virtual std::string_view name() const = 0;

    // Allokera buffrar, beräkna koefficienter. Ej realtid.
    virtual void prepare(const ProcessSpec& spec) = 0;

    // Nollställ internt tillstånd (delay-linjer, filter). Realtidssäker.
    virtual void reset() = 0;

    // Bearbeta in-place. Realtidssäker.
    virtual void process(AudioBufferView buffer) = 0;

    // Extra latens i samples som effekten introducerar (t.ex. oversampling).
    virtual int latencySamples() const { return 0; }
};

} // namespace gfx
