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

Engine::~Engine() {
    // No audio thread runs now, so this (UI) thread may act as the consumer.
    Command c;
    while (queue_.pop(c)) {
        delete c.preview;
        delete c.snapshot;
    }
    delete current_.payload;
    delete fading_.payload;
    delete snapshot_;
    for (std::size_t i = 0; i < retired_snapshot_count_; ++i) delete retired_snapshots_[i].snapshot;
    for (std::size_t i = 0; i < garbage_retry_count_; ++i) garbage_retry_[i].destroy(garbage_retry_[i].object);
    collect_garbage();
}

void Engine::pump() {
    render(nullptr, 0);
    // Only called while no audio thread runs, so touching the voices here is safe.
    for (PreviewSlot* slot : {&current_, &fading_}) {
        slot->voice.stop();
        retire(slot->payload);
        slot->payload = nullptr;
    }
    for (SoundRuntime& sound : sounds_) sound.sampler.stop_all();
    for (std::size_t i = 0; i < retired_snapshot_count_; ++i) retire_snapshot(retired_snapshots_[i].snapshot);
    retired_snapshot_count_ = 0;
    pub_previewing_.store(false, std::memory_order_relaxed);
}

void Engine::collect_garbage() {
    Garbage g;
    while (garbage_.pop(g)) g.destroy(g.object);
}

bool Engine::send(const Command& command) {
    if (queue_.push(command)) return true;
    // The engine never saw this payload, so it is still ours to free.
    delete command.preview;
    delete command.snapshot;
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

const SnapshotSound* Engine::find_sound(uint64_t id) const {
    if (!snapshot_) return nullptr;
    for (const SnapshotSound& sound : snapshot_->sounds) if (sound.id == id) return &sound;
    return nullptr;
}

Engine::SoundRuntime* Engine::runtime(uint64_t id) {
    for (SoundRuntime& sound : sounds_) if (sound.id == id) return &sound;
    for (SoundRuntime& sound : sounds_) {
        if (!sound.sampler.active()) {
            sound.id = id;
            return &sound;
        }
    }
    // There are at most 64 current sounds and room for 128 runtime slots.
    // This fallback only matters if a project replaces channels repeatedly
    // while old voices are still in their 5 ms retirement fade.
    SoundRuntime& sound = sounds_[0];
    sound.sampler.stop_all();
    sound.id = id;
    return &sound;
}

std::size_t Engine::event_cursor_for(double tick) const {
    if (!snapshot_) return 0;
    std::size_t lo = 0, hi = snapshot_->loop_events.size();
    while (lo < hi) {
        const std::size_t mid = lo + (hi - lo) / 2;
        if (static_cast<double>(snapshot_->loop_events[mid].start_tick) < tick) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

void Engine::retire_snapshot(Snapshot* snapshot) {
    if (!snapshot) return;
    const Garbage g = make_garbage(snapshot);
    if (garbage_.push(g)) return;
    if (garbage_retry_count_ < garbage_retry_.size()) {
        garbage_retry_[garbage_retry_count_++] = g;
        return;
    }
    leaked_.fetch_add(1, std::memory_order_relaxed);
    assert(!"snapshot garbage queue overflow");
}

void Engine::set_snapshot(Snapshot* next) {
    if (next == snapshot_) return;
    Snapshot* previous = snapshot_;
    const uint64_t previous_generation = snapshot_generation_;
    snapshot_ = next;
    ++snapshot_generation_;

    // Voices keep raw sample pointers. Retag a voice when the new snapshot
    // owns that same sample, and fade voices whose sample is being removed or
    // replaced; this lets the old snapshot stay alive only as long as needed.
    if (previous) {
        for (SoundRuntime& state : sounds_) {
            if (!state.sampler.generation_active(previous_generation)) continue;
            const SnapshotSound* replacement = find_sound(state.id);
            if (replacement) state.sampler.retag_generation(previous_generation, snapshot_generation_, replacement->sample.get());
            state.sampler.fade_generation(previous_generation, sample_rate_);
        }

        if (retired_snapshot_count_ == retired_snapshots_.size()) {
            const uint64_t oldest = retired_snapshots_[0].generation;
            for (SoundRuntime& state : sounds_) state.sampler.stop_generation(oldest);
            retire_snapshot(retired_snapshots_[0].snapshot);
            for (std::size_t i = 1; i < retired_snapshot_count_; ++i) retired_snapshots_[i - 1] = retired_snapshots_[i];
            --retired_snapshot_count_;
        }

        bool still_used = false;
        for (const SoundRuntime& state : sounds_) still_used |= state.sampler.generation_active(previous_generation);
        if (still_used) retired_snapshots_[retired_snapshot_count_++] = {previous, previous_generation};
        else retire_snapshot(previous);
    }

    if (snapshot_) {
        for (const SnapshotSound& sound : snapshot_->sounds) {
            SoundRuntime* state = runtime(sound.id);
            state->gain = sound.gain;
            state->pan_left = sound.pan_left;
            state->pan_right = sound.pan_right;
            state->audible = sound.audible;
        }
        if (snapshot_->loop_length_ticks > 0) loop_length_ = snapshot_->loop_length_ticks;
    }
    event_cursor_ = event_cursor_for(pos_);
}

void Engine::collect_retired_snapshots() {
    std::size_t i = 0;
    while (i < retired_snapshot_count_) {
        const uint64_t generation = retired_snapshots_[i].generation;
        bool active = false;
        for (const SoundRuntime& state : sounds_) active |= state.sampler.generation_active(generation);
        if (active) {
            ++i;
            continue;
        }
        retire_snapshot(retired_snapshots_[i].snapshot);
        for (std::size_t j = i + 1; j < retired_snapshot_count_; ++j) retired_snapshots_[j - 1] = retired_snapshots_[j];
        --retired_snapshot_count_;
    }
}

void Engine::apply(const Command& c) {
    switch (c.type) {
    case CommandType::Play:
        if (!playing_) {
            playing_ = true;
            pos_ = static_cast<double>(origin());
            next_beat_ = first_beat_at_or_after(pos_, sig_);
            event_cursor_ = event_cursor_for(pos_);
        }
        break;
    case CommandType::Stop:
        playing_ = false;
        click_.stop();
        for (SoundRuntime& sound : sounds_) sound.sampler.fade_all(sample_rate_);
        pos_ = static_cast<double>(origin());
        event_cursor_ = event_cursor_for(pos_);
        break;
    case CommandType::Seek:
        pos_ = static_cast<double>(c.a);
        next_beat_ = first_beat_at_or_after(pos_, sig_);
        event_cursor_ = event_cursor_for(pos_);
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
        event_cursor_ = event_cursor_for(pos_);
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
        event_cursor_ = event_cursor_for(pos_);
        break;
    }
    case CommandType::SetLoopLength:
        if (c.a > 0) loop_length_ = c.a;
        break;
    case CommandType::PreviewSample:
        start_preview(c.preview);
        break;
    case CommandType::StopPreview:
        start_preview(nullptr);
        break;
    case CommandType::SetSnapshot:
        set_snapshot(c.snapshot);
        break;
    case CommandType::PreviewNote: {
        if (c.a <= 0 || c.b < 0 || c.b > 127 || !std::isfinite(c.number)) break;
        const SnapshotSound* sound = find_sound(static_cast<uint64_t>(c.a));
        if (!sound || !sound->sample) break;
        runtime(sound->id)->sampler.note_on(*sound, snapshot_generation_, static_cast<int>(c.b),
                                           static_cast<float>(std::clamp(c.number, 0.0, 1.0)), -1, false,
                                           sample_rate_);
        break;
    }
    }
}

void Engine::retire(PreviewPayload* p) {
    if (!p) return;
    const Garbage g = make_garbage(p);
    if (garbage_.push(g)) return;
    // The UI hasn't emptied the queue for a while; keep it for a later try.
    if (garbage_retry_count_ < garbage_retry_.size()) {
        garbage_retry_[garbage_retry_count_++] = g;
        return;
    }
    // Never free here: losing a few bytes beats a glitch in the audio.
    leaked_.fetch_add(1, std::memory_order_relaxed);
    assert(!"garbage queue overflow");
}

void Engine::start_preview(PreviewPayload* p) {
    // At most two voices: a third preview cuts the oldest without a fade.
    if (fading_.payload) {
        fading_.voice.stop();
        retire(fading_.payload);
    }
    fading_ = current_;
    fading_.voice.fade_out(sample_rate_);
    current_ = {};
    if (p) {
        current_.payload = p;
        const Sample* s = p->sample.get();
        if (s && s->sample_rate > 0) current_.voice.start(*s, s->sample_rate / sample_rate_, p->gain);
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
    // Retry garbage the UI couldn't take last time, oldest first.
    std::size_t kept = 0;
    for (std::size_t i = 0; i < garbage_retry_count_; ++i) {
        if (!garbage_.push(garbage_retry_[i])) garbage_retry_[kept++] = garbage_retry_[i];
    }
    garbage_retry_count_ = kept;

    while (queue_.pop(c)) apply(c);

    const int64_t beat_len = ticks_per_beat(sig_);
    const int64_t bar_len = ticks_per_bar(sig_);

    // One iteration per sample. Every event (click, later notes and the loop
    // jump) is checked against the playhead of this exact sample, so the
    // result does not depend on how the audio device splits time into buffers.
    for (uint32_t i = 0; i < frames; ++i) {
        const double frame_tick = pos_;
        if (playing_) {
            if (play_mode_ == PlayMode::Loop && snapshot_) {
                while (event_cursor_ < snapshot_->loop_events.size() &&
                       static_cast<double>(snapshot_->loop_events[event_cursor_].start_tick) <= pos_) {
                    const NoteEvent& event = snapshot_->loop_events[event_cursor_++];
                    const SnapshotSound* sound = find_sound(event.sound_id);
                    if (!sound || !sound->sample || event.pitch < 0 || event.pitch > 127) continue;
                    runtime(sound->id)->sampler.note_on(*sound, snapshot_generation_, event.pitch,
                                                         event.velocity, event.end_tick, event.slide,
                                                         sample_rate_);
                }
            }
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
                    event_cursor_ = event_cursor_for(0.0);
                }
            } else if (loop_enabled_ && loop_end_ > loop_start_ && before < static_cast<double>(loop_end_) &&
                       pos_ >= static_cast<double>(loop_end_)) {
                // SONG: the brace loops only when the playhead crosses its end
                // during this sample; a playhead already past the end plays on.
                // Keep the overshoot so the loop stays in phase with the tempo.
                pos_ = static_cast<double>(loop_start_) + (pos_ - static_cast<double>(loop_end_));
                // A beat sitting on the loop start must click on the next sample.
                next_beat_ = first_beat_at_or_after(static_cast<double>(loop_start_), sig_);
                event_cursor_ = event_cursor_for(pos_);
            }
        }
        const float s = click_.next();
        float pl = 0, pr = 0, fl = 0, fr = 0;
        current_.voice.render_frame(pl, pr);
        fading_.voice.render_frame(fl, fr);
        float left = s + pl + fl;
        float right = s + pr + fr;
        for (SoundRuntime& sound : sounds_) {
            if (!sound.sampler.active()) continue;
            float sl = 0, sr = 0;
            sound.sampler.render_frame(frame_tick, sample_rate_, sl, sr);
            if (sound.audible) {
                left += sl * sound.gain * sound.pan_left;
                right += sr * sound.gain * sound.pan_right;
            }
        }
        out[2 * i] = left;
        out[2 * i + 1] = right;
        // A voice that just ended hands its payload back.
        if (current_.payload && !current_.voice.active()) {
            retire(current_.payload);
            current_.payload = nullptr;
        }
        if (fading_.payload && !fading_.voice.active()) {
            retire(fading_.payload);
            fading_.payload = nullptr;
        }
        ++frame_;
    }

    collect_retired_snapshots();

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
    bool any_sample_voice = current_.voice.active() || fading_.voice.active();
    for (const SoundRuntime& sound : sounds_) any_sample_voice |= sound.sampler.active();
    pub_previewing_.store(any_sample_voice, std::memory_order_relaxed);
}

StereoPeak Engine::take_master_peak() {
    return {pub_peak_left_.exchange(0.0f, std::memory_order_relaxed),
            pub_peak_right_.exchange(0.0f, std::memory_order_relaxed)};
}

}  // namespace aloglas
