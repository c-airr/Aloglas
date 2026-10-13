// Audio output through miniaudio: opens the default playback device and lets
// it call Engine::render() from its own real-time thread.
//
// All methods are for the UI thread. miniaudio's types stay inside the .cpp
// (pimpl), so including this header does not drag in the 90k-line miniaudio.h.
#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

namespace aloglas {

class Engine;

enum class AudioStatus { Closed, Running, NoDevice, Error };

struct AudioDeviceOptions {
    // Use miniaudio's simulated "null" device (tests): real-time pacing,
    // no hardware.
    bool null_backend = false;
};

class AudioDevice {
public:
    explicit AudioDevice(Engine& engine, AudioDeviceOptions options = {});
    ~AudioDevice();
    AudioDevice(const AudioDevice&) = delete;
    AudioDevice& operator=(const AudioDevice&) = delete;

    // Opens and starts the default output. Idempotent while running.
    // On failure the status says why and the app keeps running without audio.
    bool open();
    // Stops the stream and waits for the audio thread to finish.
    void close();

    AudioStatus status() const { return status_; }
    const std::string& error() const { return error_; }
    uint32_t sample_rate() const { return sample_rate_; }
    uint32_t buffer_frames() const { return buffer_frames_; }

    // Share of each buffer's duration spent inside Engine::render(), smoothed.
    // 0.25 means rendering takes a quarter of the time available.
    float cpu_load() const { return cpu_load_.load(std::memory_order_relaxed); }

    // True once after the device stopped without us asking (unplugged,
    // driver reset). The UI then stops the transport and closes the device.
    bool take_lost() { return lost_.exchange(false); }

    struct Impl;  // miniaudio state; public only so the C callbacks can reach it

private:
    Engine& engine_;
    AudioDeviceOptions options_;
    std::unique_ptr<Impl> impl_;
    AudioStatus status_ = AudioStatus::Closed;
    std::string error_;
    uint32_t sample_rate_ = 0;
    uint32_t buffer_frames_ = 0;

    std::atomic<float> cpu_load_{0.0f};
    std::atomic<bool> lost_{false};
    std::atomic<bool> closing_{false};

    friend struct Callbacks;
};

}  // namespace aloglas
