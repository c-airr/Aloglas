// Minimal UTF-8 helpers. Text is stored as UTF-8 std::string; a "code point"
// is one character as the user sees it for our purposes (ż is 2 bytes, 1 code
// point). Byte offsets passed around always sit on code-point boundaries.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace aloglas {

std::size_t utf8_length(std::string_view s);                     // in code points
std::size_t utf8_next(std::string_view s, std::size_t byte);      // start of the next code point
std::size_t utf8_prev(std::string_view s, std::size_t byte);      // start of the previous one
std::string utf8_truncate(std::string_view s, std::size_t max_code_points);
std::string trim(std::string_view s);                             // ASCII whitespace

}  // namespace aloglas
