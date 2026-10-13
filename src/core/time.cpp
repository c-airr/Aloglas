#include "core/time.h"

#include <cmath>
#include <cstdio>

namespace aloglas {

bool is_valid(TimeSignature sig) {
    const bool num_ok = sig.num >= 1 && sig.num <= 32;
    const bool den_ok = sig.den == 1 || sig.den == 2 || sig.den == 4 || sig.den == 8 || sig.den == 16;
    return num_ok && den_ok;
}

int64_t ticks_per_beat(TimeSignature sig) {
    // A whole note is 4 quarters; one beat is a whole note divided by den.
    return kPpq * 4 / sig.den;
}

int64_t ticks_per_bar(TimeSignature sig) {
    return sig.num * ticks_per_beat(sig);
}

int64_t max_ticks(TimeSignature sig) {
    return kMaxBars * ticks_per_bar(sig);
}

double ticks_per_sample(double bpm, double sample_rate) {
    // bpm quarters per minute -> bpm * kPpq ticks per 60 seconds.
    return bpm * static_cast<double>(kPpq) / (60.0 * sample_rate);
}

double ticks_to_seconds(double ticks, double bpm) {
    return ticks / static_cast<double>(kPpq) * 60.0 / bpm;
}

int64_t first_beat_at_or_after(double tick, TimeSignature sig) {
    const double beat = static_cast<double>(ticks_per_beat(sig));
    return static_cast<int64_t>(std::ceil(tick / beat)) * ticks_per_beat(sig);
}

BarBeatSixteenth to_bbs(double ticks, TimeSignature sig) {
    const int64_t t = ticks > 0.0 ? static_cast<int64_t>(std::floor(ticks)) : 0;
    const int64_t bar_len = ticks_per_bar(sig);
    const int64_t beat_len = ticks_per_beat(sig);
    const int64_t in_bar = t % bar_len;
    const int64_t in_beat = in_bar % beat_len;

    BarBeatSixteenth bbs;
    bbs.bar = t / bar_len + 1;
    bbs.beat = static_cast<int>(in_bar / beat_len) + 1;
    bbs.sixteenth = static_cast<int>(in_beat / kTicksPerSixteenth) + 1;
    return bbs;
}

std::string format_bbs(const BarBeatSixteenth& bbs) {
    char buf[48];
    std::snprintf(buf, sizeof buf, "%lld.%d.%d", static_cast<long long>(bbs.bar), bbs.beat, bbs.sixteenth);
    return buf;
}

std::string format_clock(double seconds) {
    const long long total_ms = seconds > 0.0 ? std::llround(seconds * 1000.0) : 0;
    const long long minutes = total_ms / 60000;
    const long long secs = (total_ms / 1000) % 60;
    const long long ms = total_ms % 1000;
    char buf[48];
    std::snprintf(buf, sizeof buf, "%02lld:%02lld.%03lld", minutes, secs, ms);
    return buf;
}

}  // namespace aloglas
