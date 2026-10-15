#include "app/frame_update.h"

#include <algorithm>

namespace aloglas {

void frame_update(Engine& engine, AudioDevice* device, Controller& controller, Log& log, LiveState& live) {
    if (device && device->take_lost()) {
        device->close();
        controller.stop();
        log.add(LogLevel::Error, "Audio device lost");
    }
    // Without an audio thread nobody would pop the command queue: it would
    // fill up and the transport would ignore every click.
    if (!device || device->status() != AudioStatus::Running) engine.pump();
    engine.collect_garbage();

    live.playing = engine.playing();
    live.commands_pending = engine.commands_pending();
    live.previewing = engine.previewing();
    live.position_ticks = engine.position_ticks();
    live.audio_disabled = device == nullptr;
    live.audio = device ? device->status() : AudioStatus::Closed;
    live.audio_error = device ? device->error() : std::string();
    live.sample_rate = device ? device->sample_rate() : 0;
    live.buffer_frames = device ? device->buffer_frames() : 0;
    live.cpu_load = device ? device->cpu_load() : 0.0f;

    const uint32_t dropped = engine.dropped_commands();
    if (dropped > live.dropped_commands) {
        log.add(LogLevel::Warning, std::to_string(dropped - live.dropped_commands) + " engine commands dropped");
    }
    live.dropped_commands = dropped;

    // shortcut: decay per frame, not per second; fine at 60 fps, revisit if frame pacing changes
    const StereoPeak peak = engine.take_master_peak();
    auto fall = [](float meter, float p) {
        const float m = std::max(p, meter * 0.85f);
        return m < 1e-4f ? 0.0f : m;
    };
    live.meter_left = fall(live.meter_left, peak.left);
    live.meter_right = fall(live.meter_right, peak.right);
}

bool needs_frames(const LiveState& live) {
    // Pending commands: Play was clicked but the audio thread hasn't started
    // yet; keep drawing so the change shows up without waiting for input.
    return live.playing || live.previewing || live.commands_pending || live.meter_left > 0.0f || live.meter_right > 0.0f;
}

std::string status_text(const LiveState& live) {
    std::string s;
    if (live.audio_disabled) s = "Audio disabled (--no-audio)";
    else if (live.audio == AudioStatus::Running)
        s = "Audio: " + std::to_string(live.sample_rate) + " Hz, " + std::to_string(live.buffer_frames) + " samples";
    else if (live.audio == AudioStatus::Error) s = "Audio error: " + live.audio_error;
    else s = "No audio \xE2\x80\x94 press Play to retry";
    if (live.dropped_commands > 0) s += " \xC2\xB7 " + std::to_string(live.dropped_commands) + " commands dropped";
    return s;
}

std::string position_text(const LiveState& live, const Document& doc) {
    return format_bbs(to_bbs(live.position_ticks, doc.signature)) + "  " +
           format_clock(ticks_to_seconds(live.position_ticks, doc.tempo));
}

}  // namespace aloglas
