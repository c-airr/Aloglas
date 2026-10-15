// Plays one Sample at a given speed: the building block of the preview (and,
// later, of every sampler voice). Audio thread only; no allocation.
#pragma once

#include "samples/sample.h"

namespace aloglas {

class SampleVoice {
public:
    // `step` = how many source frames to advance per output frame:
    // file_rate / device_rate (later also times the pitch ratio).
    void start(const Sample& sample, double step, float gain);
    // Linear fade to silence over 5 ms, then the voice stops. A hard stop
    // would click: the waveform would jump to 0 mid-cycle.
    void fade_out(double sample_rate);
    void stop() { active_ = false; }
    bool active() const { return active_; }

    // Adds nothing when inactive; otherwise writes one output frame.
    void render_frame(float& left, float& right);

private:
    float read(int64_t frame, int channel) const;  // 0 outside the file
    float interpolate(int channel) const;

    const Sample* sample_ = nullptr;
    double pos_ = 0;   // read position in source frames; double for the same reason as the playhead
    double step_ = 1;
    float gain_ = 1;
    // Fade counted in whole frames: summing 1/240 in float overshoots by one.
    int fade_total_ = 0;  // 0 = not fading
    int fade_left_ = 0;
    bool active_ = false;
};

}  // namespace aloglas
