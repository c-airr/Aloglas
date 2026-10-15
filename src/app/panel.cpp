#include "app/panel.h"

#include <algorithm>
#include <cmath>

#include "ui/canvas.h"
#include "ui/theme.h"

namespace aloglas {

void Panel::set_body(Widget& body) {
    body_ = &body;
    add(body);
}

void Panel::add_header_widget(Widget& w, float width) {
    header_items_.push_back({&w, width});
    add(w);
}

void Panel::layout(Rect r) {
    rect = r;
    const Theme& t = theme();
    Rect body = r;
    const Rect head = cut_top(body, t.panel_header_height);
    const float h = std::max(0.0f, t.control_height - 2);
    const float y = head.y + std::floor((head.h - h) / 2);
    float x = head.right() - 4;
    for (const HeaderItem& item : header_items_) {
        x -= item.width;
        item.widget->layout({std::max(head.x, x), y, std::min(item.width, head.w), h});
        x -= 4;
    }
    if (body_) body_->layout(body);
}

void Panel::draw(Canvas& c) {
    const Theme& t = theme();
    c.fill_rect(rect, t.panel);
    const Rect head{rect.x, rect.y, rect.w, std::min(rect.h, t.panel_header_height)};
    c.fill_rect(head, t.panel_header);
    c.hline(head.x, head.right(), head.bottom() - 1, t.panel_border);
    c.text({head.x + 8, head.y, head.w - 16, head.h}, title_, t.text_dim, t.font_size_small, Canvas::Align::Left, true);
    draw_children(c);
    if (focused_ == this) c.stroke_rect(rect, t.accent);
}

void Panel::on_press_within(const MouseEvent&) {
    if (focused_ == this) return;
    focused_ = this;
    invalidate();
}

}  // namespace aloglas
