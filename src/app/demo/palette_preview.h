// TEMPORARY: a static mock of the full Aloglas layout drawn with the current
// theme, used to pick a palette (keys 1/2/3 switch variants). Replaced by the
// real panels in the next plan.
#pragma once

#include "ui/widget.h"

namespace aloglas {

class PalettePreview : public Widget {
public:
    void draw(Canvas& c) override;
    bool on_key(const KeyEvent& e) override;
};

}  // namespace aloglas
