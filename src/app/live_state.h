// What the UI shows about the running engine and audio device, read once per
// frame by frame_update(). Widgets draw from this, never from the engine's
// atomics directly, so one frame shows one consistent set of values.
#pragma once

#include <cstdint>
#include <string>

#include "audio/audio_device.h"

namespace aloglas {

struct LiveState {
    bool playing = false;
    double position_ticks = 0;
    float cpu_load = 0;
    bool audio_disabled = false;  // --no-audio
    AudioStatus audio = AudioStatus::Closed;
    std::string audio_error;
    uint32_t sample_rate = 0;
    uint32_t buffer_frames = 0;
    uint32_t dropped_commands = 0;
    float meter_left = 0;  // linear peak with a falling decay
    float meter_right = 0;
};

}  // namespace aloglas
