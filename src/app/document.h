// Everything a project saves. Plain data: only the Controller changes it.
// Project-file persistence, tracks and clips are implemented in later stages.
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "core/time.h"
#include "engine/commands.h"
#include "samples/sample.h"

namespace aloglas {

// The playlist's loop brace (used in SONG mode).
struct LoopBrace {
    bool enabled = false;
    int64_t start = 0;
    int64_t end = 4 * 3840;  // bars 1-5 in 4/4
};

using SoundId = uint64_t;

struct Sound {
    SoundId id = 0;
    std::string name;  // UTF-8, usually the sample's filename
    std::filesystem::path sample_path;
    std::shared_ptr<const Sample> sample;
    int color = 0;
    float volume_db = 0;
    float pan = 0;  // -1 = left, +1 = right
    bool muted = false;
    int root = 60;  // MIDI C5
    float fine = 0;  // cents
    bool cut_itself = false;
    bool gate = false;
    double glide_ms = 80;
};

struct Note {
    SoundId sound_id = 0;
    int64_t start_tick = 0;
    int64_t length_ticks = 960;
    int pitch = 60;
    float velocity = 0.8f;
    bool slide = false;
};

struct Document {
    double tempo = 140.0;
    TimeSignature signature{4, 4};
    bool metronome = true;
    PlayMode play_mode = PlayMode::Loop;
    LoopBrace brace;
    int64_t start_marker = 0;
    bool follow = true;
    std::vector<Sound> sounds;
    std::vector<Note> loop_notes;
    SoundId next_sound_id = 1;
};

// Length of the loop (pattern) that LOOP mode repeats.
int64_t loop_length_ticks(const Document& doc);

}  // namespace aloglas
