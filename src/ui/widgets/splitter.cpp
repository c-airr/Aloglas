#include "ui/widgets/splitter.h"

namespace aloglas {

Splitter::Splitter(SplitAxis axis, std::function<float()> value, std::function<void(float)> set_value, float sign,
                   std::function<void()> on_reset)
    : axis_(axis),
      value_(std::move(value)),
      set_value_(std::move(set_value)),
      sign_(sign),
      on_reset_(std::move(on_reset)) {}

bool Splitter::on_mouse_down(const MouseEvent& e) {
    if (e.button != MouseButton::Left) return false;
    if (e.clicks == 2) {
        if (on_reset_) on_reset_();
        dragging_ = false;
        invalidate();
        return true;
    }
    dragging_ = true;
    start_pos_ = along(e.pos);
    start_value_ = value_();
    return true;
}

void Splitter::on_mouse_move(const MouseEvent& e) {
    if (!dragging_) return;
    set_value_(start_value_ + sign_ * (along(e.pos) - start_pos_));
    invalidate();
}

void Splitter::on_hover_enter() {
    if (UiHost* h = host()) h->set_cursor(axis_ == SplitAxis::Columns ? Cursor::ResizeH : Cursor::ResizeV);
}

void Splitter::on_hover_leave() {
    if (UiHost* h = host()) h->set_cursor(Cursor::Arrow);
}

}  // namespace aloglas
