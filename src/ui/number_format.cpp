#include "ui/number_format.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>

#include "core/utf8.h"

namespace aloglas {

namespace {
std::size_t nearest_allowed_index(const NumberSpec& spec, double value) {
    std::size_t best = 0;
    for (std::size_t i = 1; i < spec.allowed.size(); ++i) {
        if (std::abs(spec.allowed[i] - value) < std::abs(spec.allowed[best] - value)) best = i;
    }
    return best;
}
}  // namespace

double clamp_to_spec(const NumberSpec& spec, double value) {
    if (!spec.allowed.empty()) return spec.allowed[nearest_allowed_index(spec, value)];
    const double scale = std::pow(10.0, spec.decimals);
    const double rounded = std::round(value * scale) / scale;
    return std::clamp(rounded, spec.min, spec.max);
}

double drag_value(const NumberSpec& spec, double start, float dy_px, bool fine) {
    const double per_px = fine ? spec.fine_drag_per_px : spec.drag_per_px;
    if (!spec.allowed.empty()) {
        const auto steps = static_cast<long>(std::lround(dy_px * per_px));
        const long last = static_cast<long>(spec.allowed.size()) - 1;
        const long index = std::clamp(static_cast<long>(nearest_allowed_index(spec, start)) + steps, 0L, last);
        return spec.allowed[static_cast<std::size_t>(index)];
    }
    return clamp_to_spec(spec, start + dy_px * per_px);
}

std::optional<double> parse_number(const NumberSpec& spec, std::string_view text) {
    std::string t = trim(text);
    if (t.empty()) return std::nullopt;
    std::replace(t.begin(), t.end(), ',', '.');
    double value = 0.0;
    const char* end = t.data() + t.size();
    const auto [ptr, ec] = std::from_chars(t.data(), end, value);
    if (ec != std::errc() || ptr != end) return std::nullopt;  // "12abc" is not a number
    return clamp_to_spec(spec, value);
}

std::string format_number(const NumberSpec& spec, double value) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*f", spec.decimals, value);
    return buf;
}

}  // namespace aloglas
