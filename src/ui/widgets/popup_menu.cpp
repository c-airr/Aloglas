#include "ui/widgets/popup_menu.h"

#include <algorithm>

#include "ui/canvas.h"
#include "ui/theme.h"

namespace aloglas {

float PopupMenu::item_height() const {
    return theme().row_height + 4;
}

void PopupMenu::place() {
    std::size_t longest = 0;
    for (const std::string& s : items_) longest = std::max(longest, s.size());
    // shortcut: width from the byte count, not measured text; fine for short Latin labels, measure when menus get long
    const float w = std::max(120.0f, 7.0f * static_cast<float>(longest) + 24);
    const float h = item_height() * static_cast<float>(items_.size()) + 4;
    const float x = std::clamp(at_.x, rect.x, std::max(rect.x, rect.right() - w));
    const float y = std::clamp(at_.y, rect.y, std::max(rect.y, rect.bottom() - h));
    menu_ = {x, y, w, h};
}

void PopupMenu::open(Point at, std::vector<std::string> items, std::function<void(int)> on_choose) {
    items_ = std::move(items);
    on_choose_ = std::move(on_choose);
    at_ = at;
    hovered_ = -1;
    visible = true;
    place();
    focus();  // Escape reaches the menu
    invalidate();
}

void PopupMenu::close() {
    if (!visible) return;
    visible = false;
    if (has_focus()) host()->set_focus(nullptr);
    invalidate();
}

void PopupMenu::layout(Rect r) {
    rect = r;
    if (visible) place();
}

int PopupMenu::item_at(Point p) const {
    if (!menu_.contains(p)) return -1;
    const int i = static_cast<int>((p.y - menu_.y - 2) / item_height());
    return i >= 0 && i < static_cast<int>(items_.size()) ? i : -1;
}

bool PopupMenu::on_mouse_down(const MouseEvent& e) {
    if (!visible) return false;
    const int i = item_at(e.pos);
    // Copy before closing: the callback may open another menu.
    const auto choose = on_choose_;
    close();
    if (i >= 0 && choose) choose(i);
    return true;
}

void PopupMenu::on_mouse_move(const MouseEvent& e) {
    const int i = item_at(e.pos);
    if (i != hovered_) {
        hovered_ = i;
        invalidate();
    }
}

bool PopupMenu::on_key(const KeyEvent& e) {
    if (!visible || e.key != Key::Escape) return false;
    close();
    return true;
}

void PopupMenu::draw(Canvas& c) {
    if (!visible) return;
    const Theme& t = theme();
    c.fill_rect(menu_, t.panel_header);
    c.stroke_rect(menu_, t.panel_border);
    const float ih = item_height();
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const Rect r{menu_.x + 1, menu_.y + 2 + ih * static_cast<float>(i), menu_.w - 2, ih};
        if (static_cast<int>(i) == hovered_) c.fill_rect(r, t.selection);
        c.text({r.x + 10, r.y, r.w - 20, r.h}, items_[i], t.text, t.font_size);
    }
}

}  // namespace aloglas
