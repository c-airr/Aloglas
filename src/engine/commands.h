// Commands sent from the UI thread to the audio engine.
//
// A Command is a small plain struct (no strings), so it can be copied through
// the lock-free SpscQueue without allocating. The one pointer, `preview`,
// moves ownership of a heap payload to the engine (see engine/preview.h).
// The meaning of the generic fields depends on the type; use the named
// constructors instead of filling fields by hand.
#pragma once

#include <cstdint>

#include "core/time.h"

namespace aloglas {

struct PreviewPayload;

// LOOP plays the loop (pattern) from its start; SONG plays the playlist.
enum class PlayMode : uint8_t { Loop, Song };

enum class CommandType : uint8_t {
    Play,
    Stop,
    Seek,          // a = tick
    SetTempo,      // number = bpm
    SetSignature,  // a = numerator, b = denominator
    SetMetronome,  // flag = on
    SetLoop,       // flag = enabled, a = start tick, b = end tick
    SetStart,      // a = start marker tick
    SetPlayMode,   // flag = song
    SetLoopLength, // a = loop length in ticks
    PreviewSample, // preview = payload (owned by the engine from now on)
    StopPreview,
};

struct Command {
    CommandType type = CommandType::Stop;
    double number = 0.0;
    int64_t a = 0;
    int64_t b = 0;
    bool flag = false;
    PreviewPayload* preview = nullptr;

    static Command play() { return {CommandType::Play}; }
    static Command stop() { return {CommandType::Stop}; }
    static Command seek(int64_t tick) { return {CommandType::Seek, 0.0, tick}; }
    static Command set_tempo(double bpm) { return {CommandType::SetTempo, bpm}; }
    static Command set_signature(TimeSignature sig) {
        return {CommandType::SetSignature, 0.0, sig.num, sig.den};
    }
    static Command set_metronome(bool on) { return {CommandType::SetMetronome, 0.0, 0, 0, on}; }
    static Command set_loop(bool enabled, int64_t start, int64_t end) {
        return {CommandType::SetLoop, 0.0, start, end, enabled};
    }
    static Command set_start(int64_t tick) { return {CommandType::SetStart, 0.0, tick}; }
    static Command set_play_mode(PlayMode mode) {
        return {CommandType::SetPlayMode, 0.0, 0, 0, mode == PlayMode::Song};
    }
    static Command set_loop_length(int64_t ticks) { return {CommandType::SetLoopLength, 0.0, ticks}; }
    static Command preview_sample(PreviewPayload* owned) {
        Command c{CommandType::PreviewSample};
        c.preview = owned;
        return c;
    }
    static Command stop_preview() { return {CommandType::StopPreview}; }
};

// Anything that accepts commands: the real Engine, or a recorder in tests.
class CommandSink {
public:
    virtual ~CommandSink() = default;
    // Returns false when the command could not be queued.
    virtual bool send(const Command& command) = 0;
};

}  // namespace aloglas
