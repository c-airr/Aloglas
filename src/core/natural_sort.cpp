#include "core/natural_sort.h"

#include <cstddef>

namespace aloglas {

namespace {

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

char fold(char c) {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

}  // namespace

bool natural_less(std::string_view a, std::string_view b) {
    std::size_t i = 0;
    std::size_t j = 0;
    int tie = 0;  // first leading-zero difference, used only if all else is equal
    while (i < a.size() && j < b.size()) {
        if (is_digit(a[i]) && is_digit(b[j])) {
            // Compare digit runs as numbers without converting them (a run
            // can be longer than any integer type): skip leading zeros, then
            // the longer run is the bigger number; equal lengths compare
            // digit by digit.
            const std::size_t a_start = i;
            const std::size_t b_start = j;
            while (i < a.size() && a[i] == '0') ++i;
            while (j < b.size() && b[j] == '0') ++j;
            const std::size_t a_sig = i;
            const std::size_t b_sig = j;
            while (i < a.size() && is_digit(a[i])) ++i;
            while (j < b.size() && is_digit(b[j])) ++j;
            const std::size_t a_len = i - a_sig;
            const std::size_t b_len = j - b_sig;
            if (a_len != b_len) return a_len < b_len;
            for (std::size_t k = 0; k < a_len; ++k) {
                if (a[a_sig + k] != b[b_sig + k]) return a[a_sig + k] < b[b_sig + k];
            }
            if (tie == 0 && (i - a_start) != (j - b_start)) tie = (i - a_start) < (j - b_start) ? -1 : 1;
            continue;
        }
        const char ca = fold(a[i]);
        const char cb = fold(b[j]);
        if (ca != cb) return static_cast<unsigned char>(ca) < static_cast<unsigned char>(cb);
        ++i;
        ++j;
    }
    if (i < a.size() || j < b.size()) return i == a.size();  // the one that ran out is a prefix
    return tie < 0;
}

}  // namespace aloglas
