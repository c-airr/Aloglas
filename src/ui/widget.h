// Retained widget tree.
//
// A Widget is a rectangle that draws itself and reacts to input. Widgets form
// a tree: a parent lays out its children and draws them. The tree is
// "retained": it lives across frames (unlike immediate mode, where the UI is
// rebuilt every frame).
//
// Ownership: the `children` list never owns. Fixed children are member fields
// of their parent; dynamic children are std::unique_ptr held by the parent.
// Both kinds are registered with add().
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "ui/events.h"
#include "ui/geometry.h"

namespace aloglas {

class Canvas;
class Widget;

// What the widget tree needs from its environment (the Window, or a fake in tests).
class UiHost {
public:
    virtual ~UiHost() = default;
    virtual void invalidate() = 0;                 // something changed: redraw soon
    virtual void set_focus(Widget* w) = 0;         // nullptr clears focus
    virtual Widget* focus() const = 0;
    virtual void forget(Widget& subtree) = 0;      // drop pointers into a removed subtree
    virtual std::string clipboard_text() = 0;
    virtual void set_clipboard_text(std::string_view text) = 0;
    virtual void set_text_input(bool on) = 0;      // start/stop OS text input
};

class Widget {
public:
    Widget() = default;
    virtual ~Widget() = default;
    Widget(const Widget&) = delete;
    Widget& operator=(const Widget&) = delete;

    Rect rect;
    bool visible = true;
    bool enabled = true;

    Widget* parent() const { return parent_; }
    const std::vector<Widget*>& children() const { return children_; }

    void add(Widget& child);
    // Tells the host to forget pointers into `child` first, then detaches it.
    void remove(Widget& child);
    // Reorders the children list (e.g. to match document order).
    void set_children_order(const std::vector<Widget*>& order) { children_ = order; }

    void set_host(UiHost* host) { host_ = host; }  // root only
    UiHost* host() const;                          // walks up to the root

    void invalidate();
    void focus();
    bool has_focus() const;

    virtual void layout(Rect r) { rect = r; }
    virtual void draw(Canvas& canvas) { draw_children(canvas); }
    void draw_children(Canvas& canvas);

    virtual bool focusable() const { return false; }

    // Return true when the event was handled (stops bubbling to the parent).
    virtual bool on_mouse_down(const MouseEvent&) { return false; }
    virtual void on_mouse_up(const MouseEvent&) {}
    virtual void on_mouse_move(const MouseEvent&) {}  // while captured or hovered
    virtual bool on_wheel(const WheelEvent&) { return false; }
    virtual bool on_key(const KeyEvent&) { return false; }
    virtual bool on_text(const TextEvent&) { return false; }
    virtual void on_hover_enter() {}
    virtual void on_hover_leave() {}
    virtual void on_focus_gained() {}
    virtual void on_focus_lost() {}

private:
    Widget* parent_ = nullptr;
    std::vector<Widget*> children_;
    UiHost* host_ = nullptr;
};

// Deepest visible and enabled widget containing `p`; later siblings are on
// top of earlier ones. nullptr when `p` is outside the root.
Widget* hit_test(Widget& root, Point p);

// True if `w` is `ancestor` itself or one of its descendants.
bool contains_widget(const Widget& ancestor, const Widget& w);

}  // namespace aloglas
