#include "ui/widgets/text_field.h"

#include <algorithm>
#include <cmath>

#include "ui/theme.h"

namespace aloglas {

void TextField::begin(std::string_view initial) {
    edit_.set_text(initial);
    edit_.select_all();
    active_ = true;
    focus();
    invalidate();
}

void TextField::commit() {
    if (!active_) return;
    active_ = false;  // first: losing focus below must not commit a second time
    const std::string text = edit_.text();
    if (has_focus()) host()->set_focus(nullptr);
    invalidate();
    // Last: the callback may change the model, which may rebuild widgets.
    if (on_commit) on_commit(text);
}

void TextField::cancel() {
    if (!active_) return;
    active_ = false;
    if (has_focus()) host()->set_focus(nullptr);
    invalidate();
    if (on_cancel) on_cancel();
}

void TextField::on_focus_gained() {
    if (UiHost* h = host()) h->set_text_input(true);
}

void TextField::on_focus_lost() {
    if (UiHost* h = host()) h->set_text_input(false);
    if (active_) {
        // Clicked somewhere else: keep what was typed. Pass a copy: the
        // callback may edit or rebuild this field while using the text.
        active_ = false;
        invalidate();
        const std::string text = edit_.text();
        if (on_commit) on_commit(text);
    }
}

bool TextField::on_key(const KeyEvent& e) {
    if (!active_) return false;
    const bool shift = e.mods.shift;
    if (e.mods.ctrl) {
        switch (e.key) {
        case Key::A: edit_.select_all(); break;
        case Key::C:
            if (edit_.has_selection()) host()->set_clipboard_text(edit_.selected_text());
            break;
        case Key::X:
            if (edit_.has_selection()) {
                host()->set_clipboard_text(edit_.selected_text());
                edit_.backspace();
            }
            break;
        case Key::V: edit_.insert(host()->clipboard_text()); break;
        default: return false;  // other Ctrl shortcuts belong to the app
        }
        invalidate();
        return true;
    }
    switch (e.key) {
    case Key::Enter: commit(); return true;
    case Key::Escape: cancel(); return true;
    case Key::Backspace: edit_.backspace(); break;
    case Key::Delete: edit_.del(); break;
    case Key::Left: edit_.move_left(shift); break;
    case Key::Right: edit_.move_right(shift); break;
    case Key::Home: edit_.home(shift); break;
    case Key::End: edit_.end(shift); break;
    default: break;  // Space, letters: they arrive as text events
    }
    invalidate();
    return true;
}

bool TextField::on_text(const TextEvent& e) {
    if (!active_) return false;
    edit_.insert(e.utf8);
    invalidate();
    return true;
}

std::size_t TextField::byte_at(float x) const {
    if (stops_.empty()) return edit_.text().size();
    const float local = x - text_x_;
    // Nearest caret stop to the click.
    auto best = std::min_element(stops_.begin(), stops_.end(), [&](const CaretStop& a, const CaretStop& b) {
        return std::abs(a.x - local) < std::abs(b.x - local);
    });
    return best->byte;
}

bool TextField::on_mouse_down(const MouseEvent& e) {
    if (!active_ || e.button != MouseButton::Left) return false;
    if (e.clicks >= 2) {
        edit_.select_all();
    } else {
        edit_.set_caret(byte_at(e.pos.x), e.mods.shift);
        selecting_ = true;  // dragging from here extends the selection
    }
    invalidate();
    return true;
}

void TextField::on_mouse_move(const MouseEvent& e) {
    // Also called on plain hover; only a drag that started inside selects.
    if (!active_ || !selecting_) return;
    edit_.set_caret(byte_at(e.pos.x), true);
    invalidate();
}

void TextField::on_mouse_up(const MouseEvent&) {
    selecting_ = false;
}

void TextField::draw(Canvas& c) {
    const Theme& t = theme();
    const float size = t.font_size;
    c.fill_rect(rect, t.field_bg, 2);
    if (active_) c.stroke_rect(rect, t.accent, 1);

    stops_ = c.caret_stops(edit_.text(), size);
    auto x_of = [&](std::size_t byte) {
        for (const CaretStop& s : stops_) {
            if (s.byte == byte) return s.x;
        }
        return stops_.empty() ? 0.0f : stops_.back().x;
    };

    // Scroll horizontally so the caret stays visible in a narrow field.
    const float pad = 4;
    const float inner_w = std::max(0.0f, rect.w - 2 * pad);
    const float caret_x = x_of(edit_.caret());
    const float scroll = std::max(0.0f, caret_x - inner_w);
    text_x_ = rect.x + pad - scroll;

    c.push_clip({rect.x + 1, rect.y, std::max(0.0f, rect.w - 2), rect.h});
    if (edit_.has_selection()) {
        const float a = x_of(std::min(edit_.caret(), edit_.anchor()));
        const float b = x_of(std::max(edit_.caret(), edit_.anchor()));
        c.fill_rect({text_x_ + a, rect.y + 3, b - a, rect.h - 6}, t.selection);
    }
    // Draw unclipped-by-ellipsis: the field scrolls instead of shortening.
    const float full_w = stops_.empty() ? 0.0f : stops_.back().x;
    c.text({text_x_, rect.y, full_w + 1, rect.h}, edit_.text(), t.text, size);
    if (active_ && has_focus()) c.vline(text_x_ + caret_x, rect.y + 4, rect.bottom() - 4, t.caret);
    c.pop_clip();
}

}  // namespace aloglas
