#include "app/controller.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include "app/compile.h"
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
    send_snapshot();
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
    if (snapshot_sent_) send_snapshot();
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

void Controller::send_snapshot() {
    if (!snapshot_sent_ && doc_.sounds.empty() && doc_.loop_notes.empty()) return;
    Snapshot* snapshot = compile_snapshot(doc_).release();
    send(Command::set_snapshot(snapshot));
    snapshot_sent_ = true;
}

void Controller::snapshot_changed() {
    send_snapshot();
    changed();
}

Sound* Controller::find_sound(SoundId id) {
    const auto it = std::find_if(doc_.sounds.begin(), doc_.sounds.end(), [id](const Sound& s) { return s.id == id; });
    return it == doc_.sounds.end() ? nullptr : &*it;
}

SoundId Controller::add_sound(const std::filesystem::path& path, std::shared_ptr<const Sample> sample) {
    if (!sample || doc_.sounds.size() >= 64 || doc_.next_sound_id == 0) return 0;
    const std::filesystem::path sample_path = path.empty() ? sample->path : path;
    const std::u8string filename = sample_path.filename().u8string();
    Sound sound;
    sound.id = doc_.next_sound_id++;
    sound.name.assign(filename.begin(), filename.end());
    if (sound.name.empty()) sound.name = "Sample";
    sound.sample_path = sample_path;
    sound.sample = std::move(sample);
    sound.color = static_cast<int>(doc_.sounds.size() % 12);
    const SoundId id = sound.id;
    doc_.sounds.push_back(std::move(sound));
    snapshot_changed();
    return id;
}

bool Controller::remove_sound(SoundId id) {
    const auto it = std::find_if(doc_.sounds.begin(), doc_.sounds.end(), [id](const Sound& s) { return s.id == id; });
    if (it == doc_.sounds.end()) return false;
    doc_.sounds.erase(it);
    std::erase_if(doc_.loop_notes, [id](const Note& n) { return n.sound_id == id; });
    snapshot_changed();
    return true;
}

SoundId Controller::duplicate_sound(SoundId id) {
    const Sound* source = find_sound(id);
    if (!source || doc_.sounds.size() >= 64 || doc_.next_sound_id == 0) return 0;
    Sound copy = *source;
    copy.id = doc_.next_sound_id++;
    copy.color = static_cast<int>(doc_.sounds.size() % 12);
    const SoundId new_id = copy.id;
    doc_.sounds.push_back(std::move(copy));
    snapshot_changed();
    return new_id;
}

void Controller::set_sound_muted(SoundId id, bool muted) {
    Sound* sound = find_sound(id);
    if (!sound || sound->muted == muted) return;
    sound->muted = muted;
    snapshot_changed();
}

void Controller::set_sound_volume(SoundId id, float volume_db) {
    Sound* sound = find_sound(id);
    if (!sound || !std::isfinite(volume_db)) return;
    const float value = static_cast<float>(std::clamp(std::round(volume_db * 10.0f) / 10.0f, -60.0f, 6.0f));
    if (sound->volume_db == value) return;
    sound->volume_db = value;
    snapshot_changed();
}

void Controller::set_sound_pan(SoundId id, float pan) {
    Sound* sound = find_sound(id);
    if (!sound || !std::isfinite(pan)) return;
    const float value = std::clamp(std::round(pan * 100.0f) / 100.0f, -1.0f, 1.0f);
    if (sound->pan == value) return;
    sound->pan = value;
    snapshot_changed();
}

void Controller::set_sound_root(SoundId id, int root) {
    Sound* sound = find_sound(id);
    if (!sound) return;
    const int value = std::clamp(root, 0, 127);
    if (sound->root == value) return;
    sound->root = value;
    snapshot_changed();
}

void Controller::set_sound_fine(SoundId id, float fine) {
    Sound* sound = find_sound(id);
    if (!sound || !std::isfinite(fine)) return;
    const float value = std::clamp(fine, -100.0f, 100.0f);
    if (sound->fine == value) return;
    sound->fine = value;
    snapshot_changed();
}

void Controller::set_sound_cut_itself(SoundId id, bool enabled) {
    Sound* sound = find_sound(id);
    if (!sound || sound->cut_itself == enabled) return;
    sound->cut_itself = enabled;
    snapshot_changed();
}

void Controller::set_sound_gate(SoundId id, bool enabled) {
    Sound* sound = find_sound(id);
    if (!sound || sound->gate == enabled) return;
    sound->gate = enabled;
    snapshot_changed();
}

void Controller::set_sound_glide(SoundId id, double milliseconds) {
    Sound* sound = find_sound(id);
    if (!sound || !std::isfinite(milliseconds)) return;
    const double value = std::clamp(milliseconds, 0.0, 2000.0);
    if (sound->glide_ms == value) return;
    sound->glide_ms = value;
    snapshot_changed();
}

bool Controller::add_note(Note note) {
    if (!find_sound(note.sound_id) || note.length_ticks <= 0 || !std::isfinite(note.velocity)) return false;
    note.start_tick = std::clamp<int64_t>(note.start_tick, 0, loop_length_ticks(doc_) - 1);
    note.pitch = std::clamp(note.pitch, 0, 127);
    note.velocity = std::clamp(note.velocity, 0.0f, 1.0f);
    doc_.loop_notes.push_back(note);
    snapshot_changed();
    return true;
}

bool Controller::remove_note(std::size_t index) {
    if (index >= doc_.loop_notes.size()) return false;
    doc_.loop_notes.erase(doc_.loop_notes.begin() + static_cast<std::ptrdiff_t>(index));
    snapshot_changed();
    return true;
}

void Controller::preview_note(SoundId id, int pitch, float velocity) {
    if (!find_sound(id) || pitch < 0 || pitch > 127 || !std::isfinite(velocity)) return;
    if (ensure_audio && !ensure_audio()) return;
    send(Command::preview_note(id, pitch, std::clamp(velocity, 0.0f, 1.0f)));
}

}  // namespace aloglas
