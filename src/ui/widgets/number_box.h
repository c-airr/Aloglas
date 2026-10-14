// Numeric field in the Ableton style: drag up/down to change the value
// (Shift for fine steps), double-click to type a value.
// Like ToggleButton it holds no value of its own: `value` reads the model and
// `on_change` asks the model to change.
#pragma once

#include <functional>

#include "ui/number_format.h"
#include "ui/widgets/text_field.h"

namespace aloglas {

class NumberBox : public Widget {
public:
    explicit NumberBox(NumberSpec spec);

    std::function<double()> value;
    std::function<void(double)> on_change;

    void layout(Rect r) override;
    void draw(Canvas& c) override;
    bool on_mouse_down(const MouseEvent& e) override;
    void on_mouse_move(const MouseEvent& e) override;
    void on_mouse_up(const MouseEvent& e) override;

private:
    void start_editing();
    double current() const { return value ? value() : spec_.min; }

    NumberSpec spec_;
    TextField entry_;
    bool dragging_ = false;
    float drag_start_y_ = 0;
    double drag_start_value_ = 0;
    double last_sent_ = 0;
    bool last_fine_ = false;
};

}  // namespace aloglas
