// The float-vs-double lesson from the design discussion, kept as a test.
//
// The engine advances the playhead once per sample by a small step
// (ticks_per_sample, ~0.047 at 140 BPM / 48 kHz). A float has 24 bits of
// mantissa: the gap between neighbouring floats doubles at every power of
// two. Once that gap is bigger than twice the step, adding the step rounds
// back to the same number and the playhead stops moving while audio keeps
// playing. A double has 53 bits, so the same thing would take billions of bars.
#include <doctest.h>

#include <cmath>
#include <cstdint>

#include "core/time.h"

using namespace aloglas;

TEST_CASE("float lesson: time must be double") {
    const double sample_rate = 48000.0;
    const double step = ticks_per_sample(140.0, sample_rate);
    const double ticks_per_bar_d = static_cast<double>(ticks_per_bar({4, 4}));
    const int64_t samples_per_1000_bars = static_cast<int64_t>(1000 * ticks_per_bar_d / step);

    const float step_f = static_cast<float>(step);
    float pos_f = 0.0f;
    double pos_d = 0.0;
    double stall_bar = -1.0;

    for (int64_t n = 1; n <= samples_per_1000_bars; ++n) {
        pos_d += step;
        if (stall_bar < 0.0) {
            const float before = pos_f;
            pos_f += step_f;
            if (pos_f == before) stall_bar = (n * step) / ticks_per_bar_d;
        }
    }

    // How far the double playhead is from the exact position, in samples.
    const double exact = samples_per_1000_bars * step;
    const double double_error_samples = (pos_d - exact) / step;

    MESSAGE("float stalls at bar " << stall_bar << ", double error after 1000 bars: "
                                   << double_error_samples << " samples");
    CHECK(stall_bar > 0.0);
    CHECK(stall_bar < 300.0);
    CHECK(std::abs(double_error_samples) < 1.0);
}
