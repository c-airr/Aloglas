#include "ui/widgets/tree_view.h"

#include <algorithm>
#include <cmath>

#include "ui/canvas.h"
#include "ui/theme.h"

namespace aloglas {

namespace {
constexpr float kIndent = 12;
constexpr float kLeft = 4;  // gap before the first triangle
}  // namespace

int TreeView::visible_rows() const {
    return std::max(1, static_cast<int>(rect.h / theme().row_height));
}

int TreeView::row_at(float y) const {
    const int i = first_ + static_cast<int>(std::floor((y - rect.y) / theme().row_height));
    return i >= 0 && i < static_cast<int>(size()) ? i : -1;
}

void TreeView::scroll_to(int first) {
    const int max_first = std::max(0, static_cast<int>(size()) - visible_rows());
    first_ = std::clamp(first, 0, max_first);
}

void TreeView::select(int index) {
    selected_ = std::clamp(index, -1, static_cast<int>(size()) - 1);
    if (selected_ >= 0) {
        if (selected_ < first_) scroll_to(selected_);
        else if (selected_ >= first_ + visible_rows()) scroll_to(selected_ - visible_rows() + 1);
    }
    invalidate();
}

void TreeView::choose(int index) {
    select(index);
    if (selected_ >= 0 && on_select) on_select(static_cast<std::size_t>(selected_));
}

void TreeView::layout(Rect r) {
    rect = r;
    // The model may have shrunk (a root removed): keep both in range.
    selected_ = std::min(selected_, static_cast<int>(size()) - 1);
    scroll_to(first_);
}

bool TreeView::on_mouse_down(const MouseEvent& e) {
    focus();
    const int i = row_at(e.pos.y);
    if (i < 0) return true;
    const TreeRow r = row(static_cast<std::size_t>(i));
    if (e.button == MouseButton::Right) {
        select(i);
        if (on_context) on_context(static_cast<std::size_t>(i), e.pos);
        return true;
    }
    if (e.button != MouseButton::Left) return true;
    const float tri_x = rect.x + kLeft + r.depth * kIndent;
    const bool on_triangle = r.folder && e.pos.x >= tri_x && e.pos.x < tri_x + kIndent;
    if (on_triangle) {
        if (on_expand) on_expand(static_cast<std::size_t>(i), !r.expanded);
        return true;
    }
    if (e.clicks == 2 && r.folder) {
        if (on_expand) on_expand(static_cast<std::size_t>(i), !r.expanded);
        return true;
    }
    choose(i);
    return true;
}

bool TreeView::on_wheel(const WheelEvent& e) {
    scroll_to(first_ - static_cast<int>(std::lround(e.dy * 3)));
    invalidate();
    return true;
}

bool TreeView::on_key(const KeyEvent& e) {
    const int n = static_cast<int>(size());
    if (n == 0 || e.mods.ctrl || e.mods.alt) return false;
    const int s = selected_;
    switch (e.key) {
    case Key::Up:
        if (s > 0) choose(s - 1);
        else if (s < 0) choose(0);
        return true;
    case Key::Down:
        if (s < n - 1) choose(s + 1);
        return true;
    case Key::Right: {
        if (s < 0) return true;
        const TreeRow r = row(static_cast<std::size_t>(s));
        if (!r.folder) return true;
        if (!r.expanded) {
            if (on_expand) on_expand(static_cast<std::size_t>(s), true);
        } else if (s + 1 < n && row(static_cast<std::size_t>(s + 1)).depth > r.depth) {
            choose(s + 1);
        }
        return true;
    }
    case Key::Left: {
        if (s < 0) return true;
        const TreeRow r = row(static_cast<std::size_t>(s));
        if (r.folder && r.expanded) {
            if (on_expand) on_expand(static_cast<std::size_t>(s), false);
            return true;
        }
        for (int i = s - 1; i >= 0; --i) {
            if (row(static_cast<std::size_t>(i)).depth == r.depth - 1) {
                choose(i);
                break;
            }
        }
        return true;
    }
    default:
        return false;
    }
}

void TreeView::draw(Canvas& c) {
    const Theme& t = theme();
    const float rh = t.row_height;
    const int n = static_cast<int>(size());
    const int last = std::min(n, first_ + visible_rows() + 1);
    c.push_clip(rect);
    for (int i = first_; i < last; ++i) {
        const TreeRow r = row(static_cast<std::size_t>(i));
        const float y = rect.y + (i - first_) * rh;
        const bool sel = i == selected_;
        if (sel) c.fill_rect({rect.x, y, rect.w, rh}, has_focus() ? t.selection : with_alpha(t.selection, 45));
        const float x = rect.x + kLeft + r.depth * kIndent;
        const float cy = y + rh / 2;
        if (r.folder) {
            if (r.expanded) c.fill_triangle({x + 1, cy - 2}, {x + 8, cy - 2}, {x + 4.5f, cy + 2.5f}, t.text_dim);
            else c.fill_triangle({x + 2, cy - 3.5f}, {x + 6.5f, cy}, {x + 2, cy + 3.5f}, t.text_dim);
        }
        const Color col = r.dim ? t.text_dim : (sel || r.folder ? t.text : t.text_dim);
        c.text({x + kIndent, y, rect.right() - x - kIndent - 4, rh}, r.label, col, t.font_size);
    }
    c.pop_clip();
}

}  // namespace aloglas
