#include "ui/event_router.h"

namespace aloglas {

void EventRouter::set_focus(Widget* w) {
    if (w && !w->focusable()) return;
    if (w == focus_) return;
    Widget* old = focus_;
    focus_ = w;  // update first: callbacks may query focus()
    if (old) old->on_focus_lost();
    // on_focus_lost may have changed the tree (removing w) or moved focus
    // elsewhere; only tell w it has focus if that is still true.
    if (w && focus_ == w) w->on_focus_gained();
}

void EventRouter::mouse_down(const MouseEvent& e) {
    // One drag at a time: pressing another button while one is held is
    // ignored, so it can neither steal nor end the current drag.
    if (captured_) return;
    Widget* target = hit_test(root_, e.pos);
    // Clicking anywhere outside the focused widget ends its focus first
    // (a text field commits its edit before the click does anything else).
    if (focus_ && (!target || !contains_widget(*focus_, *target))) {
        set_focus(nullptr);
        // That commit may have rebuilt widgets: the old target may be gone.
        target = hit_test(root_, e.pos);
    }
    for (Widget* w = target; w; w = w->parent()) {
        if (w->on_mouse_down(e)) {
            captured_ = w;
            captured_button_ = e.button;
            break;
        }
    }
}

void EventRouter::mouse_up(const MouseEvent& e) {
    if (captured_) {
        if (e.button != captured_button_) return;  // not the button that started the drag
        Widget* c = captured_;
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
