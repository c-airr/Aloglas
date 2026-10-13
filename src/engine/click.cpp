#include "engine/click.h"

#include <cmath>
#include <numbers>

namespace aloglas {

namespace {
constexpr double kAccentHz = 1500.0;
constexpr double kNormalHz = 1000.0;
constexpr double kStartAmplitude = 0.5;   // -6 dBFS
constexpr double kDecaySeconds = 0.005;   // time constant: amplitude falls to 1/e every 5 ms
constexpr double kSilence = 1e-4;         // ~ -80 dB: below this the click is over
}  // namespace

void ClickVoice::prepare(double sample_rate) {
    sample_rate_ = sample_rate;
    // Exponential decay: multiplying by the same factor every sample gives
    // amp(t) = start * e^(-t / tau). Per sample that factor is e^(-1 / (tau * rate)).
    decay_ = std::exp(-1.0 / (kDecaySeconds * sample_rate));
}

void ClickVoice::trigger(bool accent) {
    const double hz = accent ? kAccentHz : kNormalHz;
    phase_ = 0.0;  // a sine starting at phase 0 starts at 0: no jump, no extra click
    phase_step_ = 2.0 * std::numbers::pi * hz / sample_rate_;
    amplitude_ = kStartAmplitude;
    active_ = true;
}

void ClickVoice::stop() {
    active_ = false;
    amplitude_ = 0.0;
}

float ClickVoice::next() {
    if (!active_) return 0.0f;
    const double s = amplitude_ * std::sin(phase_);
    phase_ += phase_step_;
    amplitude_ *= decay_;
    if (amplitude_ < kSilence) active_ = false;
    return static_cast<float>(s);
}

}  // namespace aloglas
