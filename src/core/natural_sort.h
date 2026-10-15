// Ordering for names as people expect it: "Kick 2" before "Kick 10".
#pragma once

#include <string_view>

namespace aloglas {

// Case-insensitive for ASCII letters (other UTF-8 bytes compare as bytes).
// Runs of digits compare by value; equal values with different leading zeros
// ("1" vs "01") put the shorter run first, so the order is strict.
bool natural_less(std::string_view a, std::string_view b);

}  // namespace aloglas
