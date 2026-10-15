// The only code that changes the Document. Each change is validated here,
// then forwarded to the engine as commands, then announced with on_change
// (the window redraws). Keeping every mutation in one place is what later
// makes undo, saving and "modified" tracking possible.
#pragma once

#include <cstdint>
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

private:
    void send(const Command& c) { engine_.send(c); }
    void changed() {
        if (on_change) on_change();
    }
    void send_brace() { send(Command::set_loop(doc_.brace.enabled, doc_.brace.start, doc_.brace.end)); }

    Document& doc_;
    CommandSink& engine_;
};

}  // namespace aloglas
