#include "engine/engine.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include "core/rt_guard.h"

namespace aloglas {

Engine::Engine() {
    prepare(48000.0);
}

void Engine::prepare(double sample_rate) {
    sample_rate_ = sample_rate;
    ticks_per_sample_ = ticks_per_sample(bpm_, sample_rate_);
    click_.prepare(sample_rate_);
}

bool Engine::send(const Command& command) {
    if (queue_.push(command)) return true;
    dropped_.fetch_add(1, std::memory_order_relaxed);
    // A full queue means the audio thread has not run for a long time (or the
    // UI floods it). Debug builds stop here so the cause gets investigated.
    assert(!assert_on_drop_ && "engine command queue full");
    return false;
}

// LOOP mode always starts at the loop start (tick 0). SONG mode follows FL
// Studio: with the loop brace enabled, playback starts at the brace start;
// otherwise at the start marker. Stop returns here.
int64_t Engine::origin() const {
    if (play_mode_ == PlayMode::Loop) return 0;
    if (loop_enabled_ && loop_end_ > loop_start_) return loop_start_;
    return start_marker_;
}

void Engine::apply(const Command& c) {
    switch (c.type) {
    case CommandType::Play:
        if (!playing_) {
            playing_ = true;
            pos_ = static_cast<double>(origin());
            next_beat_ = first_beat_at_or_after(pos_, sig_);
        }
        break;
    case CommandType::Stop:
        playing_ = false;
        click_.stop();
        pos_ = static_cast<double>(origin());
        break;
    case CommandType::Seek:
        pos_ = static_cast<double>(c.a);
        next_beat_ = first_beat_at_or_after(pos_, sig_);
        break;
    case CommandType::SetTempo:
        // The UI validates too, but here a bad value is fatal (NaN playhead,
        // time running backwards), so the engine never trusts the payload.
        if (!std::isfinite(c.number) || c.number <= 0.0) break;
        bpm_ = std::clamp(c.number, kMinBpm, kMaxBpm);
        ticks_per_sample_ = ticks_per_sample(bpm_, sample_rate_);
        break;
    case CommandType::SetSignature: {
        // A zero numerator or denominator would divide by zero on this thread.
        const TimeSignature sig{static_cast<int>(c.a), static_cast<int>(c.b)};
        if (c.a != sig.num || c.b != sig.den || !is_valid(sig)) break;
        sig_ = sig;
        next_beat_ = first_beat_at_or_after(pos_, sig_);
        break;
    }
    case CommandType::SetMetronome:
        metronome_ = c.flag;
        if (!metronome_) click_.stop();
        break;
    case CommandType::SetLoop:
        loop_enabled_ = c.flag;
        loop_start_ = c.a;
        loop_end_ = c.b;
        if (!playing_) pos_ = static_cast<double>(origin());
        break;
    case CommandType::SetStart:
        start_marker_ = c.a;
        if (!playing_) pos_ = static_cast<double>(origin());
        break;
    case CommandType::SetPlayMode: {
        const PlayMode mode = c.flag ? PlayMode::Song : PlayMode::Loop;
        if (mode == play_mode_) break;
        play_mode_ = mode;
        // Playing or not, continue from the new mode's origin.
        pos_ = static_cast<double>(origin());
        next_beat_ = first_beat_at_or_after(pos_, sig_);
        break;
    }
    case CommandType::SetLoopLength:
        if (c.a > 0) loop_length_ = c.a;
        break;
    }
}

void Engine::log_click(bool accent) {
    click_log_[clicks_written_ % kClickLogSize] = {frame_, accent};
    ++clicks_written_;
}

std::size_t Engine::click_count() const {
    return clicks_written_ < kClickLogSize ? clicks_written_ : kClickLogSize;
}

ClickEvent Engine::click(std::size_t i) const {
    const std::size_t oldest = clicks_written_ < kClickLogSize ? 0 : clicks_written_ - kClickLogSize;
    return click_log_[(oldest + i) % kClickLogSize];
}

void Engine::render(float* out, uint32_t frames) {
    rt::AudioThreadScope audio_thread;  // debug builds abort on allocation below

    Command c;
    while (queue_.pop(c)) apply(c);

    const int64_t beat_len = ticks_per_beat(sig_);
    const int64_t bar_len = ticks_per_bar(sig_);

    // One iteration per sample. Every event (click, later notes and the loop
    // jump) is checked against the playhead of this exact sample, so the
    // result does not depend on how the audio device splits time into buffers.
    for (uint32_t i = 0; i < frames; ++i) {
        if (playing_) {
            if (pos_ >= static_cast<double>(next_beat_)) {
                if (metronome_) {
                    const bool accent = next_beat_ % bar_len == 0;
                    click_.trigger(accent);
                    log_click(accent);
                }
                next_beat_ += beat_len;
            }
            const double before = pos_;
            pos_ += ticks_per_sample_;
            if (play_mode_ == PlayMode::Loop) {
                if (pos_ >= static_cast<double>(loop_length_)) {
                    // fmod keeps the overshoot (stays in phase with the tempo)
                    // and also folds a playhead the loop just shrank under.
                    pos_ = std::fmod(pos_, static_cast<double>(loop_length_));
                    // A beat crossed during this sample (tick 0 on a normal
                    // wrap) must still click, on the next sample.
                    next_beat_ = first_beat_at_or_after(pos_ - ticks_per_sample_, sig_);
                }
            } else if (loop_enabled_ && loop_end_ > loop_start_ && before < static_cast<double>(loop_end_) &&
                       pos_ >= static_cast<double>(loop_end_)) {
                // SONG: the brace loops only when the playhead crosses its end
                // during this sample; a playhead already past the end plays on.
                // Keep the overshoot so the loop stays in phase with the tempo.
                pos_ = static_cast<double>(loop_start_) + (pos_ - static_cast<double>(loop_end_));
                // A beat sitting on the loop start must click on the next sample.
                next_beat_ = first_beat_at_or_after(static_cast<double>(loop_start_), sig_);
            }
        }
        const float s = click_.next();
        out[2 * i] = s;
        out[2 * i + 1] = s;
        ++frame_;
    }

    float peak_l = 0.0f;
    float peak_r = 0.0f;
    for (uint32_t i = 0; i < frames; ++i) {
        peak_l = std::max(peak_l, std::fabs(out[2 * i]));
        peak_r = std::max(peak_r, std::fabs(out[2 * i + 1]));
    }
    // Load + store, not an atomic max: a peak lost to a race with
    // take_master_peak() only makes one meter frame a bit low.
    pub_peak_left_.store(std::max(pub_peak_left_.load(std::memory_order_relaxed), peak_l), std::memory_order_relaxed);
    pub_peak_right_.store(std::max(pub_peak_right_.load(std::memory_order_relaxed), peak_r), std::memory_order_relaxed);

    // relaxed is enough: each value is independent and the UI only needs a
    // recent value, not one consistent with other memory writes.
    pub_playing_.store(playing_, std::memory_order_relaxed);
    pub_position_.store(pos_, std::memory_order_relaxed);
    pub_frames_.store(frame_, std::memory_order_relaxed);
    pub_play_mode_.store(play_mode_, std::memory_order_relaxed);
}

StereoPeak Engine::take_master_peak() {
    return {pub_peak_left_.exchange(0.0f, std::memory_order_relaxed),
            pub_peak_right_.exchange(0.0f, std::memory_order_relaxed)};
}

}  // namespace aloglas
