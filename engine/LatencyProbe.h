#pragma once
// Mätning av rundturslatens (ut -> kabel -> in) direkt i appen.
//
// Användning: koppla en kabel från ljudkortets utgång till vald ingång
// (INST av, låg nivå, inga högtalare/förstärkare inkopplade) och anropa
// start(). Motorn skickar då ut ett kort klick kNumPings gånger och letar
// efter toppen på ingången. Resultatet är den totala latensen genom
// DAC + ADC + drivrutin + buffertar, i samples.
//
// Allt körs på ljudtråden utan allokering; minnet reserveras i prepare().

#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>

namespace gfx {

class LatencyProbe {
public:
    enum class Status { Idle = 0, Running, Done, NoSignal, Inconsistent };

    static constexpr int kNumPings = 5;
    static constexpr float kPingLevel = 0.25f;   // -12 dBFS
    static constexpr double kWindowSeconds = 0.5; // max mätbar latens

    void prepare(double sampleRate) {
        sr_ = sampleRate;
        window_ = static_cast<int>(kWindowSeconds * sampleRate);
        capture_.assign(static_cast<size_t>(window_), 0.0f);
        state_ = State::Off;
    }

    // Valfri tråd.
    void start() { request_.store(true, std::memory_order_release); }
    void cancel() { cancel_.store(true, std::memory_order_release); }

    Status status() const { return static_cast<Status>(status_.load(std::memory_order_acquire)); }
    int resultSamples() const { return result_.load(std::memory_order_relaxed); }
    double resultMs() const { return sr_ > 0 ? 1000.0 * resultSamples() / sr_ : 0.0; }
    bool isActive() const { return status() == Status::Running; }

    // Ljudtråden. in = gitarringång, out = signal som ska skickas ut.
    // Returnerar true om proben äger utgången detta block.
    bool process(const float* in, float* out, int n) noexcept {
        if (cancel_.exchange(false, std::memory_order_acq_rel)) {
            state_ = State::Off;
            status_.store(int(Status::Idle), std::memory_order_release);
        }
        if (request_.exchange(false, std::memory_order_acq_rel) && window_ > 0) {
            state_ = State::Silence;
            counter_ = 0;
            ping_ = 0;
            status_.store(int(Status::Running), std::memory_order_release);
        }
        if (state_ == State::Off) return false;

        for (int i = 0; i < n; ++i) {
            const float x = in[i]; // in och out får vara samma buffert
            out[i] = 0.0f;
            switch (state_) {
                case State::Silence: // låt tidigare ljud/svansar klinga av
                    if (++counter_ >= window_ / 2) { state_ = State::Ping; }
                    break;
                case State::Ping:
                    out[i] = kPingLevel;
                    capture_[0] = x;
                    counter_ = 1;
                    state_ = State::Capture;
                    break;
                case State::Capture:
                    capture_[static_cast<size_t>(counter_)] = x;
                    if (++counter_ >= window_) analyse();
                    break;
                case State::Off:
                    break;
            }
        }
        return true;
    }

private:
    enum class State { Off, Silence, Ping, Capture };

    void analyse() noexcept {
        int best = 0;
        float peak = 0.0f;
        for (int i = 0; i < window_; ++i) {
            const float a = std::fabs(capture_[static_cast<size_t>(i)]);
            if (a > peak) { peak = a; best = i; }
        }
        results_[ping_] = peak > kPingLevel * 0.02f ? best : -1; // minst -46 dB tillbaka
        ++ping_;
        counter_ = 0;
        if (ping_ < kNumPings) { state_ = State::Silence; return; }

        state_ = State::Off;
        int sorted[kNumPings];
        std::copy(results_, results_ + kNumPings, sorted);
        std::sort(sorted, sorted + kNumPings);
        if (sorted[0] < 0) { status_.store(int(Status::NoSignal), std::memory_order_release); return; }
        const int median = sorted[kNumPings / 2];
        const bool consistent = sorted[kNumPings - 1] - sorted[0] <= 2;
        result_.store(median, std::memory_order_relaxed);
        status_.store(int(consistent ? Status::Done : Status::Inconsistent), std::memory_order_release);
    }

    double sr_ = 48000.0;
    int window_ = 0, counter_ = 0, ping_ = 0;
    int results_[kNumPings] {};
    std::vector<float> capture_;
    State state_ = State::Off;
    std::atomic<bool> request_ { false }, cancel_ { false };
    std::atomic<int> status_ { int(Status::Idle) }, result_ { 0 };
};

} // namespace gfx
