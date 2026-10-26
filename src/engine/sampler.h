// Fixed-voice sample instrument. Audio-thread methods are allocation-free.
#pragma once

#include <array>
#include <cstdint>

#include "engine/snapshot.h"

namespace aloglas {

class Sampler {
public:
    static constexpr std::size_t kVoiceCount = 16;

    void note_on(const SnapshotSound& sound, uint64_t generation, int pitch, float velocity,
                 int64_t end_tick, bool slide, double device_rate);
    void fade_generation(uint64_t generation, double device_rate);
    void retag_generation(uint64_t old_generation, uint64_t new_generation, const Sample* same_sample);
    bool generation_active(uint64_t generation) const;
    bool active() const;
    void fade_all(double device_rate);
    void stop_all();
    void stop_generation(uint64_t generation);
    void render_frame(double tick, double device_rate, float& left, float& right);

private:
    struct Voice {
        const Sample* sample = nullptr;
        uint64_t generation = 0;
        uint64_t order = 0;
        double position = 0;
        double source_rate = 1;
        double semitones = 0;
        double target_semitones = 0;
        double semitone_delta = 0;
        int glide_left = 0;
        int64_t end_tick = -1;
        float velocity = 0;
        bool gate = false;
        bool gate_released = false;
        int fade_total = 0;
        int fade_left = 0;
        bool active = false;

        void start(const SnapshotSound& sound, uint64_t gen, int pitch, float vel,
                   int64_t end, bool use_gate, double device_rate, uint64_t serial);
        void fade_out(double device_rate);
        void render(double tick, double device_rate, float& left, float& right);
        float read(int64_t frame, int channel) const;
        float interpolate(int channel) const;
    };

    std::array<Voice, kVoiceCount> voices_{};
    Voice steal_tail_{};
    uint64_t next_order_ = 1;
    bool active_hint_ = false;  // avoids scanning all 16 voices for empty channels every sample
};

}  // namespace aloglas
