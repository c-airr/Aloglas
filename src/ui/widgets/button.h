// Momentary push button: on_click fires on release, and only if the press
// started inside and the release is inside (dragging off cancels, like every
// OS button).
#pragma once

#include <functional>
#include <string>

#include "ui/widget.h"

namespace aloglas {

class Button : public Widget {
public:
    explicit Button(std::string label) : label_(std::move(label)) {}

    std::function<void()> on_click;

    void draw(Canvas& c) override;
    bool on_mouse_down(const MouseEvent& e) override;
    void on_mouse_up(const MouseEvent& e) override;
    void on_hover_enter() override;
    void on_hover_leave() override;

protected:
    bool hovered_ = false;
    bool pressed_ = false;

private:
    std::string label_;
};

}  // namespace aloglas
