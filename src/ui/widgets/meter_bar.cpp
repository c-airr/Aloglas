#include "ui/widgets/meter_bar.h"

#include <algorithm>
#include <cmath>

#include "ui/canvas.h"
#include "ui/theme.h"

namespace aloglas {

namespace {
constexpr float kMinDb = -60.0f;
constexpr float kMaxDb = 6.0f;
}  // namespace

float meter_fraction(float peak) {
    if (peak <= 0.0f) return 0.0f;
    const float db = 20.0f * std::log10(peak);
    return std::clamp((db - kMinDb) / (kMaxDb - kMinDb), 0.0f, 1.0f);
}

void MeterBar::draw(Canvas& c) {
    const Theme& t = theme();
    const float zero_db = meter_fraction(1.0f) * rect.w;  // where red starts
    const float bar_h = std::max(1.0f, (rect.h - 1) / 2);
    const float values[2] = {left_ ? left_() : 0.0f, right_ ? right_() : 0.0f};
    for (int i = 0; i < 2; ++i) {
        const Rect bar{rect.x, rect.y + i * (bar_h + 1), rect.w, bar_h};
        c.fill_rect(bar, t.field_bg);
        const float w = meter_fraction(values[i]) * rect.w;
        c.fill_rect({bar.x, bar.y, std::min(w, zero_db), bar.h}, t.meter_ok);
        if (w > zero_db) c.fill_rect({bar.x + zero_db, bar.y, w - zero_db, bar.h}, t.meter_hot);
    }
}

}  // namespace aloglas
