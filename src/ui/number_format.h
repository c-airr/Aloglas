// Rules for numeric fields (tempo, time signature, ...): range, rounding,
// mouse dragging and typed input. Pure functions, tested without a window.
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aloglas {

struct NumberSpec {
    double min = 0;
    double max = 1;
    int decimals = 0;
    double drag_per_px = 1;          // value change per pixel of vertical drag
    double fine_drag_per_px = 0.1;   // the same with Shift held
    // When not empty, only these values are allowed (e.g. 1/2/4/8/16) and a
    // drag steps through the list: drag_per_px is then steps per pixel.
    std::vector<double> allowed;
};

// Clamps to [min, max] and rounds to `decimals`, or picks the nearest allowed value.
double clamp_to_spec(const NumberSpec& spec, double value);

// Value after dragging `dy_px` pixels from `start`; dy > 0 means the mouse
// moved up, which makes the value larger (as in Ableton).
double drag_value(const NumberSpec& spec, double start, float dy_px, bool fine);

// Parses typed text. Accepts ',' as the decimal separator (Polish keyboards
// type "140,5"). Returns nullopt for empty or non-numeric text; the caller
// then keeps the old value.
std::optional<double> parse_number(const NumberSpec& spec, std::string_view text);

std::string format_number(const NumberSpec& spec, double value);  // "140.00"

}  // namespace aloglas
