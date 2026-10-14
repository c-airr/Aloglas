// A line of text that asks for its content every time it is drawn, so it can
// never show a stale value.
#pragma once

#include <functional>
#include <string>

#include "ui/canvas.h"
#include "ui/widget.h"

namespace aloglas {

class Label : public Widget {
public:
    explicit Label(std::function<std::string()> text, Canvas::Align align = Canvas::Align::Left, bool bold = false)
        : text_(std::move(text)), align_(align), bold_(bold) {}

    void draw(Canvas& c) override;

private:
    std::function<std::string()> text_;
    Canvas::Align align_;
    bool bold_;
};

}  // namespace aloglas
