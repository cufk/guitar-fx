#pragma once
// Tap tempo: tryck i takt (pedal, tangent eller knapp) -> BPM.
// Medelvärde av de senaste upp till fyra intervallen. En paus på mer än 2 s börjar om.
// Ren logik utan klocka: anroparen skickar tidpunkten i ms (meddelandetråden).

#include <algorithm>
#include <array>

namespace gfx {

class TapTempo {
public:
    // Returnerar nytt tempo (40–240 BPM) från och med andra trycket, annars 0.
    float tap(double ms) {
        if (taps_ > 0 && (ms - last_ > 2000.0 || ms <= last_)) taps_ = 0;
        if (taps_ > 0) {
            intervals_[size_t(next_)] = ms - last_;
            next_ = (next_ + 1) % static_cast<int>(intervals_.size());
            count_ = std::min(count_ + 1, static_cast<int>(intervals_.size()));
        } else {
            count_ = 0;
            next_ = 0;
        }
        last_ = ms;
        ++taps_;
        if (count_ == 0) return 0.0f;
        double sum = 0.0;
        for (int i = 0; i < count_; ++i) sum += intervals_[size_t(i)];
        return std::clamp(static_cast<float>(60000.0 / (sum / count_)), 40.0f, 240.0f);
    }

private:
    std::array<double, 4> intervals_ {};
    double last_ = 0.0;
    int taps_ = 0, count_ = 0, next_ = 0;
};

} // namespace gfx
