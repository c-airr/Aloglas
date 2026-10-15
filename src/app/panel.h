// A titled panel: a thin header (title on the left, small controls on the
// right) above a body widget. Clicking anywhere inside makes it the focused
// panel, which editing shortcuts act on; it then shows an accent outline.
#pragma once

#include <string>
#include <vector>

#include "ui/widget.h"

namespace aloglas {

class Panel : public Widget {
public:
    // `focused_slot` is owned by the main view: one focused panel at a time.
    Panel(std::string title, Panel*& focused_slot) : title_(std::move(title)), focused_(focused_slot) {}

    const std::string& title() const { return title_; }
    void set_body(Widget& body);
    Widget* body() const { return body_; }
    void add_header_widget(Widget& w, float width);

    void layout(Rect r) override;
    void draw(Canvas& c) override;
    void on_press_within(const MouseEvent&) override;

private:
    struct HeaderItem {
        Widget* widget;
        float width;
    };

    std::string title_;
    Panel*& focused_;
    Widget* body_ = nullptr;
    std::vector<HeaderItem> header_items_;
};

}  // namespace aloglas
