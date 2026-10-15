#include "app/controller.h"

#include <algorithm>
#include <utility>

#include "ui/number_format.h"

namespace aloglas {

namespace {

// Brace bounds in order, inside the timeline, at least one beat long.
LoopBrace normalized(LoopBrace b, TimeSignature sig) {
    const int64_t max = max_ticks(sig);
    const int64_t beat = ticks_per_beat(sig);
    if (b.end < b.start) std::swap(b.start, b.end);
    b.start = std::clamp<int64_t>(b.start, 0, max);
    b.end = std::clamp<int64_t>(b.end, 0, max);
    if (b.end - b.start < beat) {
        b.end = b.start + beat;
        if (b.end > max) {
            b.end = max;
            b.start = max - beat;
        }
    }
    return b;
}

int64_t clamp_marker(int64_t tick, TimeSignature sig) {
    return std::clamp<int64_t>(tick, 0, max_ticks(sig) - 1);
}

}  // namespace

void Controller::sync_engine() {
    send(Command::set_tempo(doc_.tempo));
    send(Command::set_signature(doc_.signature));
    send(Command::set_metronome(doc_.metronome));
    send_brace();
    send(Command::set_start(doc_.start_marker));
    send(Command::set_play_mode(doc_.play_mode));
    send(Command::set_loop_length(loop_length_ticks(doc_)));
}

void Controller::play() {
    if (ensure_audio && !ensure_audio()) return;
    send(Command::play());
}

void Controller::stop() {
    send(Command::stop());
}

void Controller::toggle_play(bool playing_now) {
    if (playing_now) stop();
    else play();
}

void Controller::set_tempo(double bpm) {
    const double t = clamp_to_spec({20, 999, 2}, bpm);
    if (t == doc_.tempo) return;
    doc_.tempo = t;
    send(Command::set_tempo(t));
    changed();
}

void Controller::set_signature(TimeSignature sig) {
    if (!is_valid(sig) || sig == doc_.signature) return;
    doc_.signature = sig;
    // The brace and marker were valid for the old bar length; a shorter
    // timeline or longer beat can make them invalid now.
    doc_.brace = normalized(doc_.brace, sig);
    doc_.start_marker = clamp_marker(doc_.start_marker, sig);
    send(Command::set_signature(sig));
    send_brace();
    send(Command::set_start(doc_.start_marker));
    send(Command::set_loop_length(loop_length_ticks(doc_)));
    changed();
}

void Controller::set_metronome(bool on) {
    if (on == doc_.metronome) return;
    doc_.metronome = on;
    send(Command::set_metronome(on));
    changed();
}

void Controller::set_play_mode(PlayMode mode) {
    if (mode == doc_.play_mode) return;
    doc_.play_mode = mode;
    send(Command::set_play_mode(mode));
    changed();
}

void Controller::set_brace_range(int64_t start, int64_t end) {
    LoopBrace b = doc_.brace;
    b.start = start;
    b.end = end;
    b = normalized(b, doc_.signature);
    if (b.start == doc_.brace.start && b.end == doc_.brace.end) return;
    doc_.brace = b;
    send_brace();
    changed();
}

void Controller::set_brace_enabled(bool on) {
    if (on == doc_.brace.enabled) return;
    doc_.brace.enabled = on;
    send_brace();
    changed();
}

void Controller::set_start_marker(int64_t tick, bool seek_playhead) {
    const int64_t t = clamp_marker(tick, doc_.signature);
    if (t != doc_.start_marker) {
        doc_.start_marker = t;
        send(Command::set_start(t));
        changed();
    }
    if (seek_playhead) send(Command::seek(t));
}

void Controller::set_follow(bool on) {
    if (on == doc_.follow) return;
    doc_.follow = on;
    changed();
}

}  // namespace aloglas
