// Bottom line of the window: audio state and dropped commands.
#pragma once

#include "app/live_state.h"
#include "ui/widget.h"

namespace aloglas {

class StatusBar : public Widget {
public:
    explicit StatusBar(const LiveState& live) : live_(live) {}
    void draw(Canvas& c) override;

private:
    const LiveState& live_;
};

}  // namespace aloglas
