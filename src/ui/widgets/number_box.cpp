#include "ui/widgets/number_box.h"

#include "ui/theme.h"

namespace aloglas {

NumberBox::NumberBox(NumberSpec spec) : spec_(std::move(spec)) {
    add(entry_);
    entry_.visible = false;
    entry_.on_commit = [this](const std::string& text) {
        entry_.visible = false;
        invalidate();
        // Not a number: keep the old value (nothing is sent).
        if (const auto v = parse_number(spec_, text); v && on_change) on_change(*v);
    };
    entry_.on_cancel = [this] {
        entry_.visible = false;
        invalidate();
    };
}

void NumberBox::layout(Rect r) {
    rect = r;
    entry_.layout(r);
}

void NumberBox::draw(Canvas& c) {
    if (entry_.visible) {
        entry_.draw(c);
        return;
    }
    const Theme& t = theme();
    c.fill_rect(rect, dragging_ ? t.button_hover : t.field_bg);
    c.stroke_rect(rect, t.panel_border);
    c.text(rect, format_number(spec_, current()), t.text, t.font_size, Canvas::Align::Center, true);
}

void NumberBox::start_editing() {
    dragging_ = false;
    entry_.visible = true;
    entry_.begin(format_number(spec_, current()));
}

bool NumberBox::on_mouse_down(const MouseEvent& e) {
    if (e.button != MouseButton::Left) return false;
    if (e.clicks >= 2) {
        start_editing();
        return true;
    }
    dragging_ = true;
    drag_start_y_ = e.pos.y;
    drag_start_value_ = current();
    last_sent_ = drag_start_value_;
    last_fine_ = e.mods.shift;
    invalidate();
    return true;
}

void NumberBox::on_mouse_move(const MouseEvent& e) {
    if (!dragging_) return;
    if (e.mods.shift != last_fine_) {
        // Shift pressed or released mid-drag: continue from where we are,
        // otherwise the value would jump to the other speed's position.
        last_fine_ = e.mods.shift;
        drag_start_y_ = e.pos.y;
        drag_start_value_ = last_sent_;
    }
    // Screen y grows downward; dragging up must increase the value.
    const double v = drag_value(spec_, drag_start_value_, drag_start_y_ - e.pos.y, e.mods.shift);
    if (v != last_sent_) {
        last_sent_ = v;
        if (on_change) on_change(v);
        invalidate();
    }
}

void NumberBox::on_mouse_up(const MouseEvent&) {
    if (!dragging_) return;
    dragging_ = false;
    invalidate();
}

}  // namespace aloglas
