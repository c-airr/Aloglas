#include "ui/widgets/button.h"

#include "ui/canvas.h"
#include "ui/theme.h"

namespace aloglas {

void Button::draw(Canvas& c) {
    const Theme& t = theme();
    c.fill_rect(rect, pressed_ || hovered_ ? t.button_hover : t.button);
    c.stroke_rect(rect, t.panel_border);
    c.text(rect, label_, t.text, t.font_size_small, Canvas::Align::Center);
}

bool Button::on_mouse_down(const MouseEvent& e) {
    if (e.button != MouseButton::Left) return false;
    pressed_ = true;
    invalidate();
    return true;
}

void Button::on_mouse_up(const MouseEvent& e) {
    const bool fire = pressed_ && rect.contains(e.pos);
    pressed_ = false;
    invalidate();
    if (fire && on_click) on_click();
}

void Button::on_hover_enter() {
    hovered_ = true;
    invalidate();
}

void Button::on_hover_leave() {
    hovered_ = false;
    invalidate();
}

}  // namespace aloglas
