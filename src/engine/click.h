// Metronome click: a short sine burst with an exponential decay.
#pragma once

namespace aloglas {

class ClickVoice {
public:
    void prepare(double sample_rate);

    // Restarts the burst. Accented clicks (first beat of a bar) are higher.
    void trigger(bool accent);
    void stop();

    // Produces one sample; 0 when the click has died away.
    float next();
    bool active() const { return active_; }

private:
    double sample_rate_ = 48000.0;
    double decay_ = 0.0;        // amplitude multiplier per sample
    double phase_ = 0.0;        // radians
    double phase_step_ = 0.0;   // radians per sample
    double amplitude_ = 0.0;
    bool active_ = false;
};

}  // namespace aloglas
