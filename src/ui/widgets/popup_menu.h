// A right-click menu. While open it covers its parent (the main view adds it
// as the last child, on top of everything), so a click anywhere either picks
// an item or just closes the menu, never reaching the widget underneath.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "ui/widget.h"

namespace aloglas {

class PopupMenu : public Widget {
public:
    PopupMenu() { visible = false; }

    void open(Point at, std::vector<std::string> items, std::function<void(int)> on_choose);
    void close();
    bool is_open() const { return visible; }
    Rect menu_rect() const { return menu_; }

    bool focusable() const override { return visible; }
    void layout(Rect r) override;
    void draw(Canvas& c) override;
    bool on_mouse_down(const MouseEvent& e) override;
    void on_mouse_move(const MouseEvent& e) override;
    bool on_key(const KeyEvent& e) override;

private:
    float item_height() const;
    int item_at(Point p) const;  // -1 outside the items
    void place();                // computes menu_ from at_, kept inside rect

    std::vector<std::string> items_;
    std::function<void(int)> on_choose_;
    Point at_;
    Rect menu_;
    int hovered_ = -1;
};

}  // namespace aloglas
