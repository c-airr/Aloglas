// Shortening text to fit a width: "Kick 808 long" -> "Kick…".
#pragma once

#include <functional>
#include <string>
#include <string_view>

namespace aloglas {

// Returns `text` if it fits in `max_width`, otherwise the longest prefix (cut
// at a code point) followed by "…" that fits, or "" if not even "…" fits.
// `measure` returns the rendered width of a string.
std::string fit_text(std::string_view text, float max_width,
                     const std::function<float(std::string_view)>& measure);

}  // namespace aloglas
