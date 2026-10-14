// On/off button. It does not store the state: `value` reads it from the model
// every time, and a click asks the model to flip it through `on_toggle`.
#pragma once

#include <functional>
#include <string>

#include "ui/widgets/button.h"

namespace aloglas {

class ToggleButton : public Button {
public:
    explicit ToggleButton(std::string label);

    std::function<bool()> value;
    std::function<void(bool)> on_toggle;
    // Optional icon drawn instead of the label (e.g. play triangle).
    std::function<void(Canvas&, Rect, bool on)> draw_icon;

    void draw(Canvas& c) override;

private:
    std::string label_;
};

}  // namespace aloglas
