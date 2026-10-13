// Points and rectangles in logical pixels, plus "rect cutting" for layout:
// a panel takes strips off the edges of the remaining space, one by one.
#pragma once

#include <algorithm>

namespace aloglas {

struct Point {
    float x = 0;
    float y = 0;
};

struct Rect {
    float x = 0;
    float y = 0;
    float w = 0;
    float h = 0;

    float right() const { return x + w; }
    float bottom() const { return y + h; }
    // Half-open: the right and bottom edges belong to the next rect, so two
    // adjacent rects never both contain the same point.
    bool contains(Point p) const { return p.x >= x && p.x < x + w && p.y >= y && p.y < y + h; }
    bool empty() const { return w <= 0 || h <= 0; }
};

// Each cut_* removes a strip of size `a` from one side of `r` (shrinking r)
// and returns the strip. `a` is clamped to what is available, so sizes never
// go negative even in a tiny window.
inline Rect cut_left(Rect& r, float a) {
    a = std::clamp(a, 0.0f, std::max(r.w, 0.0f));
    const Rect strip{r.x, r.y, a, r.h};
    r.x += a;
    r.w -= a;
    return strip;
}

inline Rect cut_right(Rect& r, float a) {
    a = std::clamp(a, 0.0f, std::max(r.w, 0.0f));
    r.w -= a;
    return {r.x + r.w, r.y, a, r.h};
}

inline Rect cut_top(Rect& r, float a) {
    a = std::clamp(a, 0.0f, std::max(r.h, 0.0f));
    const Rect strip{r.x, r.y, r.w, a};
    r.y += a;
    r.h -= a;
    return strip;
}

inline Rect cut_bottom(Rect& r, float a) {
    a = std::clamp(a, 0.0f, std::max(r.h, 0.0f));
    r.h -= a;
    return {r.x, r.y + r.h, r.w, a};
}

inline Rect inset(Rect r, float d) {
    const float w = std::max(0.0f, r.w - 2 * d);
    const float h = std::max(0.0f, r.h - 2 * d);
    return {r.x + (r.w - w) / 2, r.y + (r.h - h) / 2, w, h};
}

}  // namespace aloglas
