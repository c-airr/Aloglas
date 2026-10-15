// Everything a project saves. Plain data: only the Controller changes it.
// Step 2 holds the transport part; sounds, the loop's notes, tracks and clips
// are added by the steps that need them.
#pragma once

#include <cstdint>

#include "core/time.h"
#include "engine/commands.h"

namespace aloglas {

// The playlist's loop brace (used in SONG mode).
struct LoopBrace {
    bool enabled = false;
    int64_t start = 0;
    int64_t end = 4 * 3840;  // bars 1-5 in 4/4
};

struct Document {
    double tempo = 140.0;
    TimeSignature signature{4, 4};
    bool metronome = true;
    PlayMode play_mode = PlayMode::Loop;
    LoopBrace brace;
    int64_t start_marker = 0;
    bool follow = true;
};

// Length of the loop (pattern) that LOOP mode repeats.
int64_t loop_length_ticks(const Document& doc);

}  // namespace aloglas
