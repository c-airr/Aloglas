// Text editing model behind TextField: text, caret and selection, with no
// drawing. Kept separate so the editing rules can be tested without a window.
//
// The selection is the range between `anchor` and `caret` (either order).
// Without a selection both are equal. Offsets are bytes on UTF-8 boundaries.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace aloglas {

class TextEdit {
public:
    explicit TextEdit(std::size_t max_chars = 64) : max_chars_(max_chars) {}

    const std::string& text() const { return text_; }
    std::size_t caret() const { return caret_; }
    std::size_t anchor() const { return anchor_; }
    bool has_selection() const { return caret_ != anchor_; }
    std::string selected_text() const;

    void set_text(std::string_view text);   // caret to the end, no selection
    void insert(std::string_view utf8);     // replaces the selection; respects max_chars
    void backspace();
    void del();

    // `select` = Shift held: extend the selection instead of moving both ends.
    void move_left(bool select);
    void move_right(bool select);
    void home(bool select);
    void end(bool select);
    void select_all();
    void set_caret(std::size_t byte, bool select);

private:
    void erase_selection();
    std::size_t sel_begin() const { return caret_ < anchor_ ? caret_ : anchor_; }
    std::size_t sel_end() const { return caret_ < anchor_ ? anchor_ : caret_; }

    std::string text_;
    std::size_t caret_ = 0;
    std::size_t anchor_ = 0;
    std::size_t max_chars_;
};

}  // namespace aloglas
