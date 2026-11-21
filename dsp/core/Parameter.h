#pragma once
// Trådsäker parameter + mjuk övergång på ljudtråden.
//
// GUI/MIDI-tråden anropar set(). Ljudtråden läser via SmoothedValue som
// glider linjärt mot målvärdet, så att rattvridningar och preset-byten
// inte ger klick.

#include <algorithm>
#include <atomic>
#include <cmath>

namespace gfx {

class Parameter {
public:
    Parameter(float defaultValue, float minValue, float maxValue)
        : min_(minValue), max_(maxValue), value_(std::clamp(defaultValue, minValue, maxValue)) {}

    void  set(float v) noexcept { value_.store(std::clamp(v, min_, max_), std::memory_order_relaxed); }
    float get() const noexcept  { return value_.load(std::memory_order_relaxed); }
    float min() const noexcept  { return min_; }
    float max() const noexcept  { return max_; }

private:
    float min_, max_;
    std::atomic<float> value_;
    static_assert(std::atomic<float>::is_always_lock_free, "atomic<float> måste vara låsfri");
};

class SmoothedValue {
public:
    void prepare(double sampleRate, double rampSeconds) noexcept {
        rampLength_ = std::max(1, static_cast<int>(sampleRate * rampSeconds));
        countdown_ = 0;
        current_ = target_;
    }

    void setCurrentAndTarget(float v) noexcept { current_ = target_ = v; countdown_ = 0; }

    void setTarget(float v) noexcept {
        if (!(v < target_) && !(v > target_)) return; // == utan -Wfloat-equal
        target_ = v;
        countdown_ = rampLength_;
        step_ = (target_ - current_) / static_cast<float>(rampLength_);
    }

    float next() noexcept {
        if (countdown_ <= 0) return target_;
        --countdown_;
        current_ = (countdown_ == 0) ? target_ : current_ + step_;
        return current_;
    }

    bool  isSmoothing() const noexcept { return countdown_ > 0; }
    float current() const noexcept { return countdown_ > 0 ? current_ : target_; }
    float target() const noexcept  { return target_; }

private:
    float current_ = 0.0f, target_ = 0.0f, step_ = 0.0f;
    int   countdown_ = 0, rampLength_ = 1;
};

} // namespace gfx
