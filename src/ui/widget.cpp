#include "ui/widget.h"

#include <algorithm>
#include <ranges>

namespace aloglas {

void Widget::add(Widget& child) {
    child.parent_ = this;
    children_.push_back(&child);
}

void Widget::remove(Widget& child) {
    if (UiHost* h = host()) h->forget(child);
    std::erase(children_, &child);
    child.parent_ = nullptr;
}

UiHost* Widget::host() const {
    const Widget* w = this;
    while (w->parent_) w = w->parent_;
    return w->host_;
}

void Widget::invalidate() {
    if (UiHost* h = host()) h->invalidate();
}

void Widget::focus() {
    if (UiHost* h = host()) h->set_focus(this);
}

bool Widget::has_focus() const {
    const UiHost* h = host();
    return h && h->focus() == this;
}

void Widget::draw_children(Canvas& canvas) {
    for (Widget* child : children_) {
        if (child->visible) child->draw(canvas);
    }
}

Widget* hit_test(Widget& root, Point p) {
    if (!root.visible || !root.enabled || !root.rect.contains(p)) return nullptr;
    // Reverse order: the child added last is drawn last, so it is on top.
    for (Widget* child : std::views::reverse(root.children())) {
        if (Widget* hit = hit_test(*child, p)) return hit;
    }
    return &root;
}

bool contains_widget(const Widget& ancestor, const Widget& w) {
    for (const Widget* p = &w; p; p = p->parent()) {
        if (p == &ancestor) return true;
    }
    return false;
}

}  // namespace aloglas
