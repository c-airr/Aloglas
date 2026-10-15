// Body of the LOG panel: time + message per row, newest at the bottom.
// It follows new lines until the user scrolls up; then the visible lines
// stay put while new ones arrive.
#pragma once

#include <cstdint>

#include "app/log.h"
#include "ui/widget.h"
#include "ui/widgets/button.h"

namespace aloglas {

class LogPanel : public Widget {
public:
    explicit LogPanel(Log& log);

    // Not a child: the panel header hosts it.
    Button& clear_button() { return clear_; }
    // In lines; 0 = following the newest line.
    int scroll_from_bottom() const { return scroll_; }

    void layout(Rect r) override;
    void draw(Canvas& c) override;
    bool on_wheel(const WheelEvent& e) override;

private:
    void sync();  // account for lines added since the last call, clamp
    int visible_rows() const;

    Log& log_;
    Button clear_{"Clear"};
    int scroll_ = 0;
    uint64_t seen_ = 0;
};

}  // namespace aloglas
