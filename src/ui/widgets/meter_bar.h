// Stereo peak meter: two thin horizontal bars (left on top), dB scale.
#pragma once

#include <functional>

#include "ui/widget.h"

namespace aloglas {

// Linear peak (1.0 = 0 dBFS) -> bar fill 0..1 on a -60..+6 dB scale.
float meter_fraction(float peak);

class MeterBar : public Widget {
public:
    MeterBar(std::function<float()> left, std::function<float()> right)
        : left_(std::move(left)), right_(std::move(right)) {}
    void draw(Canvas& c) override;

private:
    std::function<float()> left_;
    std::function<float()> right_;
};

}  // namespace aloglas
