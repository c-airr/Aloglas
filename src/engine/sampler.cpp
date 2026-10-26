#include "engine/sampler.h"

#include <algorithm>
#include <cmath>

namespace aloglas {

namespace {
constexpr float kFadeSeconds = 0.005f;
}

void Sampler::Voice::start(const SnapshotSound& sound, uint64_t gen, int pitch, float vel,
                           int64_t end, bool use_gate, double device_rate, uint64_t serial) {
    sample = sound.sample.get();
    generation = gen;
    order = serial;
    position = 0;
    source_rate = sample && device_rate > 0 ? static_cast<double>(sample->sample_rate) / device_rate : 1.0;
    semitones = static_cast<double>(pitch - sound.root) + sound.fine / 100.0;
    target_semitones = semitones;
    semitone_delta = 0;
    glide_left = 0;
    end_tick = end;
    velocity = std::clamp(vel, 0.0f, 1.0f);
    gate = use_gate;
    gate_released = false;
    fade_total = 0;
    fade_left = 0;
    active = sample && sample->frame_count() > 0 && sample->sample_rate > 0;
}

void Sampler::Voice::fade_out(double device_rate) {
    if (!active || fade_total > 0) return;
    fade_total = std::max(1, static_cast<int>(std::lround(kFadeSeconds * device_rate)));
    fade_left = fade_total;
}

float Sampler::Voice::read(int64_t frame, int channel) const {
    if (!sample || frame < 0 || frame >= sample->frame_count()) return 0.0f;
    return sample->frames[static_cast<std::size_t>(frame * sample->channels + channel)];
}

float Sampler::Voice::interpolate(int channel) const {
    const double whole = std::floor(position);
    const auto i = static_cast<int64_t>(whole);
    const auto t = static_cast<float>(position - whole);
    const float xm1 = read(i - 1, channel);
    const float x0 = read(i, channel);
    const float x1 = read(i + 1, channel);
    const float x2 = read(i + 2, channel);
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}

void Sampler::Voice::render(double tick, double device_rate, float& left, float& right) {
    if (!active) return;
    if (gate && !gate_released && end_tick >= 0 && tick >= static_cast<double>(end_tick)) {
        gate_released = true;
        fade_out(device_rate);
    }
    if (glide_left > 0) {
        semitones += semitone_delta;
        if (--glide_left == 0) semitones = target_semitones;
    }
    const double step = source_rate * std::exp2(semitones / 12.0);
    const float fade = fade_total > 0 ? static_cast<float>(fade_left) / static_cast<float>(fade_total) : 1.0f;
    const float gain = velocity * fade;
    const float l = interpolate(0) * gain;
    left += l;
    right += sample->channels == 2 ? interpolate(1) * gain : l;
    position += step;
    if (position > static_cast<double>(sample->frame_count() - 1)) active = false;
    if (fade_total > 0 && --fade_left <= 0) active = false;
}

void Sampler::note_on(const SnapshotSound& sound, uint64_t generation, int pitch, float velocity,
                      int64_t end_tick, bool slide, double device_rate) {
    if (!sound.sample || device_rate <= 0) return;
    Voice* sounding = nullptr;
    if (slide && sound.cut_itself) {
        for (Voice& voice : voices_) {
            if (voice.active && voice.fade_total == 0 && (!sounding || voice.order > sounding->order)) sounding = &voice;
        }
        if (steal_tail_.active && steal_tail_.fade_total == 0 &&
            (!sounding || steal_tail_.order > sounding->order)) sounding = &steal_tail_;
    }
    const double target = static_cast<double>(pitch - sound.root) + sound.fine / 100.0;
    if (sounding) {
        sounding->target_semitones = target;
        const int glide_frames = static_cast<int>(std::lround(std::max(0.0, sound.glide_ms) * device_rate / 1000.0));
        sounding->glide_left = glide_frames;
        sounding->semitone_delta = glide_frames > 0 ? (target - sounding->semitones) / glide_frames : 0;
        if (glide_frames == 0) sounding->semitones = target;
        sounding->end_tick = end_tick;
        sounding->gate = sound.gate && end_tick >= 0;
        sounding->gate_released = false;
        sounding->velocity = std::clamp(velocity, 0.0f, 1.0f);
        return;
    }

    if (sound.cut_itself) {
        for (Voice& voice : voices_) voice.fade_out(device_rate);
        steal_tail_.fade_out(device_rate);
    }

    Voice* slot = nullptr;
    for (Voice& voice : voices_) {
        if (!voice.active) {
            slot = &voice;
            break;
        }
    }
    if (!slot) {
        slot = &*std::min_element(voices_.begin(), voices_.end(),
                                  [](const Voice& a, const Voice& b) { return a.order < b.order; });
        if (steal_tail_.active) steal_tail_.active = false;
        steal_tail_ = *slot;
        steal_tail_.fade_out(device_rate);
    }
    slot->start(sound, generation, pitch, velocity, end_tick, sound.gate && end_tick >= 0,
                device_rate, next_order_++);
    active_hint_ = true;
}

void Sampler::fade_generation(uint64_t generation, double device_rate) {
    for (Voice& voice : voices_) if (voice.active && voice.generation == generation) voice.fade_out(device_rate);
    if (steal_tail_.active && steal_tail_.generation == generation) steal_tail_.fade_out(device_rate);
}

void Sampler::retag_generation(uint64_t old_generation, uint64_t new_generation, const Sample* same_sample) {
    for (Voice& voice : voices_) {
        if (voice.active && voice.generation == old_generation && voice.sample == same_sample) voice.generation = new_generation;
    }
    if (steal_tail_.active && steal_tail_.generation == old_generation && steal_tail_.sample == same_sample)
        steal_tail_.generation = new_generation;
}

bool Sampler::generation_active(uint64_t generation) const {
    for (const Voice& voice : voices_) if (voice.active && voice.generation == generation) return true;
    return steal_tail_.active && steal_tail_.generation == generation;
}

bool Sampler::active() const {
    return active_hint_;
}

void Sampler::fade_all(double device_rate) {
    for (Voice& voice : voices_) voice.fade_out(device_rate);
    steal_tail_.fade_out(device_rate);
}

void Sampler::stop_all() {
    for (Voice& voice : voices_) voice.active = false;
    steal_tail_.active = false;
    active_hint_ = false;
}

void Sampler::stop_generation(uint64_t generation) {
    for (Voice& voice : voices_) if (voice.generation == generation) voice.active = false;
    if (steal_tail_.generation == generation) steal_tail_.active = false;
    active_hint_ = steal_tail_.active;
    for (const Voice& voice : voices_) active_hint_ |= voice.active;
}

void Sampler::render_frame(double tick, double device_rate, float& left, float& right) {
    active_hint_ = false;
    for (Voice& voice : voices_) {
        voice.render(tick, device_rate, left, right);
        active_hint_ |= voice.active;
    }
    steal_tail_.render(tick, device_rate, left, right);
    active_hint_ |= steal_tail_.active;
}

}  // namespace aloglas
