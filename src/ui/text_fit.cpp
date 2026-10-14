#include "ui/text_fit.h"

#include "core/utf8.h"

namespace aloglas {

std::string fit_text(std::string_view text, float max_width,
                     const std::function<float(std::string_view)>& measure) {
    if (measure(text) <= max_width) return std::string(text);

    constexpr std::string_view kEllipsis = "\xE2\x80\xA6";  // "…" in UTF-8
    std::string best;
    if (measure(kEllipsis) > max_width) return best;
    best = kEllipsis;

    // Grow the prefix one code point at a time while it still fits. Labels
    // are short, so a linear scan is plenty fast.
    std::size_t end = 0;
    while (end < text.size()) {
        const std::size_t next = utf8_next(text, end);
        std::string candidate(text.substr(0, next));
        candidate += kEllipsis;
        if (measure(candidate) > max_width) break;
        best = std::move(candidate);
        end = next;
    }
    return best;
}

}  // namespace aloglas
