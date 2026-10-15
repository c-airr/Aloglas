// Per-frame glue between the engine/device and the UI.
#pragma once

#include <string>

#include "app/controller.h"
#include "app/live_state.h"
#include "app/log.h"
#include "audio/audio_device.h"
#include "engine/engine.h"

namespace aloglas {

// Handles a lost device, keeps the engine's command queue drained when no
// audio thread runs, and refreshes `live`. `device` is null with --no-audio.
void frame_update(Engine& engine, AudioDevice* device, Controller& controller, Log& log, LiveState& live);

// True while something moves on screen (playhead, falling meters).
bool needs_frames(const LiveState& live);

std::string status_text(const LiveState& live);
std::string position_text(const LiveState& live, const Document& doc);  // "2.2.1  00:02.500"

}  // namespace aloglas
