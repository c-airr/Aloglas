// Immutable document state consumed by the audio thread. The UI allocates
// snapshots; the engine returns replaced ones through the garbage queue.
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "samples/sample.h"

namespace aloglas {

struct SnapshotSound {
    uint64_t id = 0;
    std::shared_ptr<const Sample> sample;
    float gain = 1;
    float pan_left = 0.70710678f;
    float pan_right = 0.70710678f;
    bool audible = true;
    int root = 60;
    float fine = 0;
    bool cut_itself = false;
    bool gate = false;
    double glide_ms = 80;
};

struct NoteEvent {
    uint64_t sound_id = 0;
    int64_t start_tick = 0;
    int64_t end_tick = 960;
    int pitch = 60;
    float velocity = 0.8f;
    bool slide = false;
};

struct Snapshot {
    int64_t loop_length_ticks = 3840;
    std::vector<SnapshotSound> sounds;
    std::vector<NoteEvent> loop_events;
};

}  // namespace aloglas
