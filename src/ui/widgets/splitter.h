// Draggable divider between two panels. It holds no size of its own: `value`
// reads the size it controls and `set_value` asks the owner to change it (the
// owner clamps). Double-click calls `on_reset` (the owner restores its default).
#pragma once

#include <functional>

#include "ui/widget.h"

namespace aloglas {

// Columns: a vertical divider between columns, dragged in x.
// Rows: a horizontal divider between rows, dragged in y.
enum class SplitAxis { Columns, Rows };

class Splitter : public Widget {
public:
    // `sign` turns mouse movement into a size change: +1 when dragging right/down
    // makes the size larger, -1 when it makes it smaller (a panel on the right).
    Splitter(SplitAxis axis, std::function<float()> value, std::function<void(float)> set_value, float sign,
             std::function<void()> on_reset);

    void draw(Canvas&) override {}  // the 1-px gap between the panels is the visible line
    bool on_mouse_down(const MouseEvent& e) override;
    void on_mouse_move(const MouseEvent& e) override;
    void on_hover_enter() override;
    void on_hover_leave() override;

private:
    float along(Point p) const { return axis_ == SplitAxis::Columns ? p.x : p.y; }

    SplitAxis axis_;
    std::function<float()> value_;
    std::function<void(float)> set_value_;
    float sign_;
    std::function<void()> on_reset_;
    bool dragging_ = false;
    float start_pos_ = 0;
    float start_value_ = 0;
};

}  // namespace aloglas
