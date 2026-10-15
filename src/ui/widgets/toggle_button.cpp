#include "ui/widgets/toggle_button.h"

#include "ui/canvas.h"
#include "ui/theme.h"

namespace aloglas {

ToggleButton::ToggleButton(std::string label) : Button(label), label_(std::move(label)) {
    on_click = [this] {
        if (on_toggle) on_toggle(!(value && value()));
    };
}

void ToggleButton::draw(Canvas& c) {
    const Theme& t = theme();
    const bool on = value && value();
    const Color bg = on ? t.button_on : (pressed_ || hovered_ ? t.button_hover : t.button);
    c.fill_rect(rect, bg);
    c.stroke_rect(rect, t.panel_border);
    if (draw_icon) {
        draw_icon(c, rect, on);
    } else {
        c.text(rect, label_, on ? t.accent_text : t.text, t.font_size_small, Canvas::Align::Center, on);
    }
}

}  // namespace aloglas
