#include "ui/event_router.h"

namespace aloglas {

void EventRouter::set_focus(Widget* w) {
    if (w && !w->focusable()) return;
    if (w == focus_) return;
    Widget* old = focus_;
    focus_ = w;  // update first: callbacks may query focus()
    if (old) old->on_focus_lost();
    if (w) w->on_focus_gained();
}

void EventRouter::mouse_down(const MouseEvent& e) {
    Widget* target = hit_test(root_, e.pos);
    // Clicking anywhere outside the focused widget ends its focus first
    // (a text field commits its edit before the click does anything else).
    if (focus_ && (!target || !contains_widget(*focus_, *target))) set_focus(nullptr);
    for (Widget* w = target; w; w = w->parent()) {
        if (w->on_mouse_down(e)) {
            captured_ = w;
            break;
        }
    }
}

void EventRouter::mouse_up(const MouseEvent& e) {
    if (Widget* c = captured_) {
        captured_ = nullptr;
        c->on_mouse_up(e);
    }
    update_hover(e.pos);
}

void EventRouter::mouse_move(const MouseEvent& e) {
    if (captured_) {
        captured_->on_mouse_move(e);
        return;
    }
    update_hover(e.pos);
    if (hovered_) hovered_->on_mouse_move(e);
}

void EventRouter::mouse_left_window() {
    if (captured_) return;  // a drag continues until the button is released
    if (Widget* h = hovered_) {
        hovered_ = nullptr;
        h->on_hover_leave();
    }
}

void EventRouter::update_hover(Point p) {
    Widget* h = hit_test(root_, p);
    if (h == hovered_) return;
    Widget* old = hovered_;
    hovered_ = h;
    if (old) old->on_hover_leave();
    if (h) h->on_hover_enter();
}

bool EventRouter::wheel(const WheelEvent& e) {
    for (Widget* w = hit_test(root_, e.pos); w; w = w->parent()) {
        if (w->on_wheel(e)) return true;
    }
    return false;
}

bool EventRouter::key(const KeyEvent& e) {
    for (Widget* w = focus_ ? focus_ : &root_; w; w = w->parent()) {
        if (w->on_key(e)) return true;
    }
    return false;
}

bool EventRouter::text(const TextEvent& e) {
    return focus_ && focus_->on_text(e);
}

void EventRouter::forget(Widget& subtree) {
    if (focus_ && contains_widget(subtree, *focus_)) focus_ = nullptr;
    if (captured_ && contains_widget(subtree, *captured_)) captured_ = nullptr;
    if (hovered_ && contains_widget(subtree, *hovered_)) hovered_ = nullptr;
}

}  // namespace aloglas
