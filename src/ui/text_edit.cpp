#include "ui/text_edit.h"

#include <algorithm>

#include "core/utf8.h"

namespace aloglas {

std::string TextEdit::selected_text() const {
    return text_.substr(sel_begin(), sel_end() - sel_begin());
}

void TextEdit::set_text(std::string_view text) {
    text_ = utf8_truncate(text, max_chars_);
    caret_ = anchor_ = text_.size();
}

void TextEdit::erase_selection() {
    if (!has_selection()) return;
    const std::size_t b = sel_begin();
    text_.erase(b, sel_end() - b);
    caret_ = anchor_ = b;
}

void TextEdit::insert(std::string_view utf8) {
    erase_selection();
    // Single-line field: drop control characters (newlines, tabs from a paste).
    std::string clean;
    for (char c : utf8) {
        const auto u = static_cast<unsigned char>(c);
        if (u >= 0x20 && u != 0x7F) clean.push_back(c);
    }
    const std::size_t used = utf8_length(text_);
    const std::size_t room = used < max_chars_ ? max_chars_ - used : 0;
    const std::string piece = utf8_truncate(clean, room);
    text_.insert(caret_, piece);
    caret_ += piece.size();
    anchor_ = caret_;
}

void TextEdit::backspace() {
    if (has_selection()) {
        erase_selection();
        return;
    }
    if (caret_ == 0) return;
    const std::size_t p = utf8_prev(text_, caret_);
    text_.erase(p, caret_ - p);
    caret_ = anchor_ = p;
}

void TextEdit::del() {
    if (has_selection()) {
        erase_selection();
        return;
    }
    if (caret_ >= text_.size()) return;
    const std::size_t n = utf8_next(text_, caret_);
    text_.erase(caret_, n - caret_);
}

void TextEdit::move_left(bool select) {
    if (!select && has_selection()) {
        caret_ = anchor_ = sel_begin();  // collapse to the left edge, like every text box
        return;
    }
    caret_ = utf8_prev(text_, caret_);
    if (!select) anchor_ = caret_;
}

void TextEdit::move_right(bool select) {
    if (!select && has_selection()) {
        caret_ = anchor_ = sel_end();
        return;
    }
    caret_ = utf8_next(text_, caret_);
    if (!select) anchor_ = caret_;
}

void TextEdit::home(bool select) {
    caret_ = 0;
    if (!select) anchor_ = caret_;
}

void TextEdit::end(bool select) {
    caret_ = text_.size();
    if (!select) anchor_ = caret_;
}

void TextEdit::select_all() {
    anchor_ = 0;
    caret_ = text_.size();
}

void TextEdit::set_caret(std::size_t byte, bool select) {
    byte = std::min(byte, text_.size());
    // Snap back to the start of a code point if we landed inside one.
    while (byte > 0 && byte < text_.size() && (static_cast<unsigned char>(text_[byte]) & 0xC0) == 0x80) --byte;
    caret_ = byte;
    if (!select) anchor_ = caret_;
}

}  // namespace aloglas
