// A scrolling tree of rows (the browser). It holds no data: `count` and
// `row` read the model each time, and changes go back through the on_*
// callbacks, like every other widget in the toolkit.
#pragma once

#include <cstddef>
#include <functional>
#include <string>

#include "ui/widget.h"

namespace aloglas {

struct TreeRow {
    int depth = 0;
    bool folder = false;
    bool expanded = false;
    bool dim = false;  // e.g. a folder that is missing on disk
    std::string label;
};

class TreeView : public Widget {
public:
    std::function<std::size_t()> count;
    std::function<TreeRow(std::size_t)> row;
    std::function<void(std::size_t)> on_select;               // click or arrow keys
    std::function<void(std::size_t)> on_activate;              // Enter on the selected row
    std::function<void(std::size_t, bool expand)> on_expand;  // triangle, double-click, Left/Right
    std::function<void(std::size_t, Point)> on_context;       // right click

    int selected() const { return selected_; }
    void select(int index);  // clamped to [-1, count - 1], scrolled into view
    int first_visible() const { return first_; }

    bool focusable() const override { return true; }
    void layout(Rect r) override;
    void draw(Canvas& c) override;
    bool on_mouse_down(const MouseEvent& e) override;
    bool on_wheel(const WheelEvent& e) override;
    bool on_key(const KeyEvent& e) override;

private:
    std::size_t size() const { return count ? count() : 0; }
    int visible_rows() const;
    int row_at(float y) const;  // -1 when below the last row
    void scroll_to(int first);
    void choose(int index);  // select + on_select

    int selected_ = -1;
    int first_ = 0;  // index of the top visible row
};

}  // namespace aloglas
