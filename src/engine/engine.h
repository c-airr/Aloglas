// The audio engine: transport, sample-accurate clock and metronome.
//
// Threading contract:
//   - send() and the state getters are called from the UI thread.
//   - render() is called from the audio thread (or from tests, offline).
//   - prepare() and pump() may only be called while no audio thread runs.
// The two threads share nothing except the command queue and a few atomics.
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "core/spsc_queue.h"
#include "core/time.h"
#include "engine/click.h"
#include "engine/commands.h"
#include "engine/garbage.h"
#include "engine/preview.h"
#include "engine/sampler.h"
#include "engine/sample_voice.h"
#include "engine/snapshot.h"

namespace aloglas {

struct StereoPeak {
    float left = 0;
    float right = 0;
};

struct ClickEvent {
    int64_t frame = 0;     // engine frame counter when the click started
    bool accent = false;
};

class Engine final : public CommandSink {
public:
    Engine();
    // Frees preview payloads and snapshots it still holds or that wait in the queues.
    // Precondition: no audio thread runs (the device is closed).
    ~Engine() override;

    void prepare(double sample_rate);

    // UI thread. Returns false (and counts a drop) when the queue is full.
    bool send(const Command& command) override;

    // Audio thread: fills `frames` interleaved stereo frames.
    // `out` may be null only when frames == 0.
    void render(float* out, uint32_t frames);

    // Applies queued commands without rendering audio. Only valid while no
    // audio thread is running (no device, device closed).
    // Also drops one-shot voices: without an audio thread they cannot finish
    // (0 frames are rendered), and must not resume when a device returns.
    void pump();

    // UI thread: deletes everything the audio thread has handed back.
    void collect_garbage();
    bool previewing() const { return pub_previewing_.load(std::memory_order_relaxed); }
    // Garbage that could not be queued even after retrying (should stay 0).
    uint32_t leaked_garbage() const { return leaked_.load(std::memory_order_relaxed); }

    // UI-thread view of the engine state (published at the end of render()).
    bool playing() const { return pub_playing_.load(std::memory_order_relaxed); }
    double position_ticks() const { return pub_position_.load(std::memory_order_relaxed); }
    uint32_t dropped_commands() const { return dropped_.load(std::memory_order_relaxed); }
    int64_t frames_rendered() const { return pub_frames_.load(std::memory_order_relaxed); }
    PlayMode play_mode() const { return pub_play_mode_.load(std::memory_order_relaxed); }
    // UI thread: commands sent but not yet applied by render().
    bool commands_pending() const { return !queue_.drained(); }

    // Highest absolute sample per channel since the last call (for the
    // master meter); resets both to 0.
    StereoPeak take_master_peak();

    // Tests disable the debug assert so they can fill the queue on purpose.
    void set_assert_on_drop(bool on) { assert_on_drop_ = on; }

    // Click log for tests and diagnostics: the last kClickLogSize clicks.
    // Read only while no render() is running.
    static constexpr std::size_t kClickLogSize = 4096;
    std::size_t click_count() const;
    ClickEvent click(std::size_t i) const;  // 0 = oldest kept
    void clear_click_log() { clicks_written_ = 0; }

private:
    void apply(const Command& c);
    int64_t origin() const;  // where Play starts and Stop returns to
    void log_click(bool accent);
    void set_snapshot(Snapshot* snapshot);
    void retire_snapshot(Snapshot* snapshot);
    void collect_retired_snapshots();
    std::size_t event_cursor_for(double tick) const;
    const SnapshotSound* find_sound(uint64_t id) const;
    struct SoundRuntime {
        uint64_t id = 0;
        Sampler sampler;
        float gain = 1;
        float pan_left = 0.70710678f;
        float pan_right = 0.70710678f;
        bool audible = false;
    };
    SoundRuntime* runtime(uint64_t id);
    static constexpr std::size_t kRuntimeCount = 128;
    static constexpr std::size_t kRetiredSnapshotCount = 8;

    SpscQueue<Command, 256> queue_;
    bool assert_on_drop_ = true;

    // --- audio-thread state (only render()/apply() touch these) ---
    double sample_rate_ = 48000.0;
    double bpm_ = 120.0;
    TimeSignature sig_{4, 4};
    bool metronome_ = true;
    bool loop_enabled_ = false;
    int64_t loop_start_ = 0;
    int64_t loop_end_ = 0;
    int64_t start_marker_ = 0;
    // Song by default so the engine alone behaves as a plain metronome; the
    // document sends its own mode (LOOP) at startup.
    PlayMode play_mode_ = PlayMode::Song;
    int64_t loop_length_ = 3840;  // one 4/4 bar

    bool playing_ = false;
    double pos_ = 0.0;            // playhead in ticks; double, never float
    double ticks_per_sample_ = 0.0;
    int64_t next_beat_ = 0;       // tick of the next metronome click
    int64_t frame_ = 0;           // frames rendered since construction
    ClickVoice click_;

    // Snapshot ownership remains on the UI side: replaced snapshots return
    // through the same audio-to-UI garbage queue as preview payloads.
    Snapshot* snapshot_ = nullptr;
    uint64_t snapshot_generation_ = 0;
    struct RetiredSnapshot {
        Snapshot* snapshot = nullptr;
        uint64_t generation = 0;
    };
    std::array<RetiredSnapshot, kRetiredSnapshotCount> retired_snapshots_{};
    std::size_t retired_snapshot_count_ = 0;
    std::array<SoundRuntime, kRuntimeCount> sounds_{};
    std::size_t event_cursor_ = 0;

    // Preview: the voice that plays and the one fading out after it.
    struct PreviewSlot {
        SampleVoice voice;
        PreviewPayload* payload = nullptr;
    };
    void start_preview(PreviewPayload* p);  // nullptr = just stop
    void retire(PreviewPayload* p);         // hand back to the UI thread
    PreviewSlot current_;
    PreviewSlot fading_;
    SpscQueue<Garbage, 256> garbage_;  // audio -> UI
    std::array<Garbage, 8> garbage_retry_{};
    std::size_t garbage_retry_count_ = 0;

    std::array<ClickEvent, kClickLogSize> click_log_{};
    std::size_t clicks_written_ = 0;

    // --- published to the UI thread ---
    std::atomic<bool> pub_playing_{false};
    std::atomic<double> pub_position_{0.0};
    std::atomic<int64_t> pub_frames_{0};
    std::atomic<bool> pub_previewing_{false};
    std::atomic<uint32_t> leaked_{0};
    std::atomic<uint32_t> dropped_{0};
    std::atomic<PlayMode> pub_play_mode_{PlayMode::Song};
    std::atomic<float> pub_peak_left_{0.0f};
    std::atomic<float> pub_peak_right_{0.0f};

    // A non-lock-free atomic would hide a mutex inside, which the audio
    // thread must never take. On x86-64 these are plain loads and stores.
    static_assert(std::atomic<double>::is_always_lock_free);
    static_assert(std::atomic<int64_t>::is_always_lock_free);
    static_assert(std::atomic<float>::is_always_lock_free);
    static_assert(std::atomic<PlayMode>::is_always_lock_free);
};

}  // namespace aloglas
