// Single-line text entry used for inline editing (track names, typed BPM).
//
// Life cycle: begin(text) selects everything and takes keyboard focus.
// Enter or clicking elsewhere commits (on_commit), Escape cancels (on_cancel).
// While focused it consumes every key except Ctrl shortcuts other than
// Ctrl+A/C/V/X, so Space types a space instead of starting playback, but
// Ctrl+T still reaches the app.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "ui/canvas.h"
#include "ui/text_edit.h"
#include "ui/widget.h"

namespace aloglas {

class TextField : public Widget {
public:
    explicit TextField(std::size_t max_chars = 64) : edit_(max_chars) {}

    void begin(std::string_view initial);
    bool active() const { return active_; }
    TextEdit& edit() { return edit_; }

    std::function<void(const std::string&)> on_commit;
    std::function<void()> on_cancel;

    bool focusable() const override { return true; }
    void draw(Canvas& c) override;
    bool on_mouse_down(const MouseEvent& e) override;
    void on_mouse_move(const MouseEvent& e) override;
    void on_mouse_up(const MouseEvent& e) override;
    bool on_key(const KeyEvent& e) override;
    bool on_text(const TextEvent& e) override;
    void on_focus_gained() override;
    void on_focus_lost() override;

private:
    void commit();
    void cancel();
    std::size_t byte_at(float x) const;

    TextEdit edit_;
    bool active_ = false;
    bool selecting_ = false;
    // Caret positions from the last draw (NanoVG can only measure text while
    // drawing), used to turn a click position into a caret position.
    std::vector<CaretStop> stops_;
    float text_x_ = 0;   // screen x of the text start in the last draw
};

}  // namespace aloglas
