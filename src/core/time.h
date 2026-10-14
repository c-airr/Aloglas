// Musical time: ticks, time signatures and conversions for display.
//
// A tick is the smallest unit of musical time: 960 per quarter note (PPQ).
// Positions stored in the document are whole ticks (int64_t), so they stay on
// their bar when the tempo changes. The running playhead is a double (see
// tests/float_lesson_test.cpp for why it can't be a float).
#pragma once

#include <cstdint>
#include <string>

namespace aloglas {

inline constexpr int64_t kPpq = 960;               // ticks per quarter note
inline constexpr int64_t kTicksPerSixteenth = kPpq / 4;
inline constexpr int kMaxBars = 1000;              // length of the timeline
inline constexpr double kMinBpm = 20.0;
inline constexpr double kMaxBpm = 999.0;

struct TimeSignature {
    int num = 4;  // beats per bar
    int den = 4;  // which note is one beat: 4 = quarter, 8 = eighth, ...

    bool operator==(const TimeSignature&) const = default;
};

// num 1..32, den one of 1, 2, 4, 8, 16.
bool is_valid(TimeSignature sig);

// One beat is one unit of the denominator: a quarter in 4/4, an eighth in 6/8.
int64_t ticks_per_beat(TimeSignature sig);
int64_t ticks_per_bar(TimeSignature sig);
int64_t max_ticks(TimeSignature sig);

// How far the playhead moves per audio sample. BPM always counts quarter
// notes, independent of the time signature (as in Ableton and FL).
double ticks_per_sample(double bpm, double sample_rate);
double ticks_to_seconds(double ticks, double bpm);

// Smallest beat position (multiple of ticks_per_beat) that is >= tick.
int64_t first_beat_at_or_after(double tick, TimeSignature sig);

// 1-based position as shown in the transport: "bar.beat.sixteenth".
struct BarBeatSixteenth {
    int64_t bar = 1;
    int beat = 1;
    int sixteenth = 1;
};

BarBeatSixteenth to_bbs(double ticks, TimeSignature sig);
std::string format_bbs(const BarBeatSixteenth& bbs);   // "12.3.2"
std::string format_clock(double seconds);              // "mm:ss.mmm", minutes not capped

}  // namespace aloglas
