#include "core/utf8.h"

namespace aloglas {

namespace {
// In UTF-8 every byte of a multi-byte character after the first looks like
// 10xxxxxx. Skipping those bytes moves by whole characters.
bool is_continuation(char c) {
    return (static_cast<unsigned char>(c) & 0xC0) == 0x80;
}
}  // namespace

std::size_t utf8_length(std::string_view s) {
    std::size_t n = 0;
    for (char c : s) n += is_continuation(c) ? 0 : 1;
    return n;
}

std::size_t utf8_next(std::string_view s, std::size_t byte) {
    if (byte >= s.size()) return s.size();
    ++byte;
    while (byte < s.size() && is_continuation(s[byte])) ++byte;
    return byte;
}

std::size_t utf8_prev(std::string_view s, std::size_t byte) {
    if (byte == 0) return 0;
    if (byte > s.size()) byte = s.size();
    --byte;
    while (byte > 0 && is_continuation(s[byte])) --byte;
    return byte;
}

std::string utf8_truncate(std::string_view s, std::size_t max_code_points) {
    std::size_t byte = 0;
    for (std::size_t i = 0; i < max_code_points && byte < s.size(); ++i) byte = utf8_next(s, byte);
    return std::string(s.substr(0, byte));
}

std::string trim(std::string_view s) {
    constexpr std::string_view ws = " \t\r\n\f\v";
    const auto first = s.find_first_not_of(ws);
    if (first == std::string_view::npos) return {};
    const auto last = s.find_last_not_of(ws);
    return std::string(s.substr(first, last - first + 1));
}

}  // namespace aloglas
