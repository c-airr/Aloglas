#include "audio/audio_device.h"

#include <chrono>

#include <miniaudio.h>

#include "engine/engine.h"

namespace aloglas {

struct AudioDevice::Impl {
    ma_context context{};
    bool context_ok = false;
    ma_device device{};
    bool device_ok = false;
};

// The C callbacks miniaudio calls on its audio thread. They reach the
// AudioDevice through the device's pUserData pointer.
struct Callbacks {
    static void data(ma_device* device, void* output, const void* /*input*/, ma_uint32 frames) {
        auto* self = static_cast<AudioDevice*>(device->pUserData);
        const auto t0 = std::chrono::steady_clock::now();
        self->engine_.render(static_cast<float*>(output), frames);
        const auto t1 = std::chrono::steady_clock::now();

        // Load = time spent / time available for this buffer. Smoothed with a
        // one-pole filter so the meter does not flicker. Only this thread
        // writes the value, so load + store is safe without a CAS loop.
        const double spent = std::chrono::duration<double>(t1 - t0).count();
        const double available = static_cast<double>(frames) / device->sampleRate;
        const float sample = static_cast<float>(spent / available);
        const float old = self->cpu_load_.load(std::memory_order_relaxed);
        self->cpu_load_.store(old + 0.1f * (sample - old), std::memory_order_relaxed);
    }

    static void notification(const ma_device_notification* n) {
        auto* self = static_cast<AudioDevice*>(n->pDevice->pUserData);
        if (n->type == ma_device_notification_type_stopped && !self->closing_.load()) {
            self->lost_.store(true);
        }
    }
};

AudioDevice::AudioDevice(Engine& engine, AudioDeviceOptions options)
    : engine_(engine), options_(options), impl_(std::make_unique<Impl>()) {}

AudioDevice::~AudioDevice() {
    close();
    if (impl_->context_ok) ma_context_uninit(&impl_->context);
}

bool AudioDevice::open() {
    if (status_ == AudioStatus::Running) return true;

    if (!impl_->context_ok) {
        ma_backend null_backend[] = {ma_backend_null};
        const ma_result r = options_.null_backend
                                ? ma_context_init(null_backend, 1, nullptr, &impl_->context)
                                : ma_context_init(nullptr, 0, nullptr, &impl_->context);
        if (r != MA_SUCCESS) {
            status_ = AudioStatus::Error;
            error_ = ma_result_description(r);
            return false;
        }
        impl_->context_ok = true;
    }

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = 0;  // 0 = the device's native rate, no resampling inside miniaudio
    config.dataCallback = &Callbacks::data;
    config.notificationCallback = &Callbacks::notification;
    config.pUserData = this;

    const ma_result r = ma_device_init(&impl_->context, &config, &impl_->device);
    if (r != MA_SUCCESS) {
        status_ = (r == MA_NO_DEVICE || r == MA_DOES_NOT_EXIST) ? AudioStatus::NoDevice : AudioStatus::Error;
        error_ = ma_result_description(r);
        return false;
    }
    impl_->device_ok = true;
    sample_rate_ = impl_->device.sampleRate;
    buffer_frames_ = impl_->device.playback.internalPeriodSizeInFrames;

    // prepare() must run before the stream starts: at this point miniaudio's
    // audio thread is not calling render() yet, so nothing can race with it.
    engine_.prepare(static_cast<double>(sample_rate_));

    closing_.store(false);
    lost_.store(false);
    if (ma_device_start(&impl_->device) != MA_SUCCESS) {
        ma_device_uninit(&impl_->device);
        impl_->device_ok = false;
        status_ = AudioStatus::Error;
        error_ = "failed to start the audio device";
        return false;
    }
    status_ = AudioStatus::Running;
    error_.clear();
    return true;
}

void AudioDevice::close() {
    if (!impl_->device_ok) return;
    closing_.store(true);  // the "stopped" notification below is ours, not a lost device
    ma_device_uninit(&impl_->device);  // stops the stream and joins the audio thread
    impl_->device_ok = false;
    status_ = AudioStatus::Closed;
}

}  // namespace aloglas
