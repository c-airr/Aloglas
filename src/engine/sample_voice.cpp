#include "engine/sample_voice.h"

#include <algorithm>
#include <cmath>

namespace aloglas {

void SampleVoice::start(const Sample& sample, double step, float gain) {
    sample_ = &sample;
    pos_ = 0;
    step_ = step;
    gain_ = gain;
    fade_total_ = 0;
    fade_left_ = 0;
    active_ = sample.frame_count() > 0;
}

void SampleVoice::fade_out(double sample_rate) {
    if (!active_ || fade_total_ > 0) return;
    fade_total_ = std::max(1, static_cast<int>(std::lround(0.005 * sample_rate)));
    fade_left_ = fade_total_;
}

float SampleVoice::read(int64_t frame, int channel) const {
    if (frame < 0 || frame >= sample_->frame_count()) return 0.0f;
    return sample_->frames[static_cast<std::size_t>(frame * sample_->channels + channel)];
}

// 4-point cubic Hermite (Catmull-Rom): a smooth curve through the frames
// around the read position. Plain linear interpolation would be cheaper but
// dulls the highs and adds aliasing when the speed changes.
float SampleVoice::interpolate(int channel) const {
    const double whole = std::floor(pos_);
    const auto i = static_cast<int64_t>(whole);
    const auto t = static_cast<float>(pos_ - whole);
    const float xm1 = read(i - 1, channel);
    const float x0 = read(i, channel);
    const float x1 = read(i + 1, channel);
    const float x2 = read(i + 2, channel);
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}

void SampleVoice::render_frame(float& left, float& right) {
    if (!active_) return;
    const float fade = fade_total_ > 0 ? static_cast<float>(fade_left_) / static_cast<float>(fade_total_) : 1.0f;
    const float g = gain_ * fade;
    const float l = interpolate(0) * g;
    left = l;
    right = sample_->channels == 2 ? interpolate(1) * g : l;

    pos_ += step_;
    if (pos_ > static_cast<double>(sample_->frame_count() - 1)) active_ = false;
    if (fade_total_ > 0 && --fade_left_ <= 0) active_ = false;
}

}  // namespace aloglas
