// Delivers input events to widgets: hit-testing, mouse capture, hover,
// keyboard focus and bubbling. Knows nothing about SDL; the Window feeds it.
#pragma once

#include "ui/events.h"
#include "ui/widget.h"

namespace aloglas {

class EventRouter {
public:
    explicit EventRouter(Widget& root) : root_(root) {}

    // Mouse down goes to the widget under the cursor and bubbles up until a
    // widget handles it; that widget then "captures" the mouse and receives
    // all moves and the matching up, even outside its rect (drags).
    void mouse_down(const MouseEvent& e);
    void mouse_up(const MouseEvent& e);
    void mouse_move(const MouseEvent& e);
    void mouse_left_window();
    bool wheel(const WheelEvent& e);
    // Keys start at the focused widget (or the root) and bubble up.
    bool key(const KeyEvent& e);
    // Text goes only to the focused widget.
    bool text(const TextEvent& e);

    Widget* focus() const { return focus_; }
    void set_focus(Widget* w);
    Widget* captured() const { return captured_; }
    Widget* hovered() const { return hovered_; }

    // Clears focus/capture/hover pointers that point into `subtree`, without
    // calling any callbacks (the widgets are about to go away).
    void forget(Widget& subtree);

private:
    void update_hover(Point p);

    Widget& root_;
    Widget* focus_ = nullptr;
    Widget* captured_ = nullptr;
    Widget* hovered_ = nullptr;
};

}  // namespace aloglas
