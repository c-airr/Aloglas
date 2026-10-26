// The only code that changes the Document. Each change is validated here,
// then forwarded to the engine as commands, then announced with on_change
// (the window redraws). Keeping every mutation in one place is what later
// makes undo, saving and "modified" tracking possible.
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>

#include "app/document.h"
#include "engine/commands.h"

namespace aloglas {

class Controller {
public:
    Controller(Document& doc, CommandSink& engine) : doc_(doc), engine_(engine) {}

    const Document& doc() const { return doc_; }

    // Opens the audio device if needed; false = no audio, so Play is pointless.
    std::function<bool()> ensure_audio;
    std::function<void()> on_change;

    // Sends the whole document state (startup, after the device reopens).
    void sync_engine();

    void play();
    void stop();
    void toggle_play(bool playing_now);

    void set_tempo(double bpm);
    void set_signature(TimeSignature sig);
    void set_metronome(bool on);
    void set_play_mode(PlayMode mode);
    void set_brace_range(int64_t start, int64_t end);
    void set_brace_enabled(bool on);
    void toggle_brace() { set_brace_enabled(!doc_.brace.enabled); }
    // Seeks the playhead too when `seek_playhead` (clicking the ruler while playing).
    void set_start_marker(int64_t tick, bool seek_playhead);
    void set_follow(bool on);

    SoundId add_sound(const std::filesystem::path& path, std::shared_ptr<const Sample> sample);
    bool remove_sound(SoundId id);
    SoundId duplicate_sound(SoundId id);
    void set_sound_muted(SoundId id, bool muted);
    void set_sound_volume(SoundId id, float volume_db);
    void set_sound_pan(SoundId id, float pan);
    void set_sound_root(SoundId id, int root);
    void set_sound_fine(SoundId id, float fine);
    void set_sound_cut_itself(SoundId id, bool enabled);
    void set_sound_gate(SoundId id, bool enabled);
    void set_sound_glide(SoundId id, double milliseconds);
    bool add_note(Note note);
    bool remove_note(std::size_t index);
    void preview_note(SoundId id, int pitch, float velocity = 0.8f);

private:
    void send(const Command& c) { engine_.send(c); }
    void send_snapshot();
    Sound* find_sound(SoundId id);
    void snapshot_changed();
    void changed() {
        if (on_change) on_change();
    }
    void send_brace() { send(Command::set_loop(doc_.brace.enabled, doc_.brace.start, doc_.brace.end)); }

    Document& doc_;
    CommandSink& engine_;
    bool snapshot_sent_ = false;
};

}  // namespace aloglas
