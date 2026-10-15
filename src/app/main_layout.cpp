#include "app/main_layout.h"

#include <algorithm>
#include <cmath>

namespace aloglas {

namespace {

// Removes the 1-px gap on the right/bottom of a panel (the divider line).
Rect without_right_gap(Rect r) {
    r.w = std::max(0.0f, r.w - 1);
    return r;
}
Rect without_bottom_gap(Rect r) {
    r.h = std::max(0.0f, r.h - 1);
    return r;
}

// Grab areas `s` px thick, centred on a 1-px gap starting at `gap`.
Rect vertical_grab(float gap, float y, float h, float s) {
    return {gap + 0.5f - s / 2, y, s, h};
}
Rect horizontal_grab(float x, float gap, float w, float s) {
    return {x, gap + 0.5f - s / 2, w, s};
}

float clamp_or_middle(float v, float lo, float hi) {
    if (hi < lo) return std::max(0.0f, (lo + hi) / 2);
    return std::clamp(v, lo, hi);
}

}  // namespace

MainLayout compute_layout(Rect window, const PanelSizes& s, const Theme& t) {
    MainLayout l;
    Rect area = window;
    l.transport = cut_top(area, t.transport_height);
    l.status = cut_bottom(area, t.status_height);

    const Rect browser_col = cut_left(area, s.browser_width);
    l.browser = without_right_gap(browser_col);
    l.split_browser = vertical_grab(l.browser.right(), area.y, area.h, t.splitter);

    const Rect piano = cut_bottom(area, std::round(area.h * s.piano_share));
    l.piano = piano;
    Rect top = without_bottom_gap(area);
    l.split_top_piano = horizontal_grab(piano.x, top.bottom(), piano.w, t.splitter);

    const Rect right = cut_right(top, s.right_width);
    l.playlist = without_right_gap(top);
    l.split_right = vertical_grab(l.playlist.right(), right.y, right.h, t.splitter);

    Rect rack = right;
    l.log = cut_bottom(rack, std::round(right.h * (1 - s.rack_share)));
    l.rack = without_bottom_gap(rack);
    l.split_rack_log = horizontal_grab(right.x, l.rack.bottom(), right.w, t.splitter);
    return l;
}

PanelSizes clamp_sizes(PanelSizes s, Rect window, const Theme& t, PanelMinimums min) {
    const float w = window.w;
    // +1: the playlist gives its right column a 1-px gap.
    s.browser_width = clamp_or_middle(s.browser_width, min.browser, w - min.right - min.playlist - 1);
    s.right_width = clamp_or_middle(s.right_width, min.right, w - s.browser_width - min.playlist - 1);

    const float h = std::max(0.0f, window.h - t.transport_height - t.status_height);
    if (h > 0) s.piano_share = clamp_or_middle(s.piano_share, min.piano / h, 1 - min.top / h);
    s.piano_share = std::clamp(s.piano_share, 0.0f, 1.0f);

    const float right_h = h - std::round(h * s.piano_share) - 1;
    if (right_h > 0) s.rack_share = clamp_or_middle(s.rack_share, min.rack / right_h, 1 - min.log / right_h);
    s.rack_share = std::clamp(s.rack_share, 0.0f, 1.0f);
    return s;
}

}  // namespace aloglas
