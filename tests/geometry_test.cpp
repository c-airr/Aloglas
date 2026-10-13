#include <doctest.h>

#include "ui/geometry.h"

using namespace aloglas;

TEST_CASE("cut and inset never go negative") {
    Rect r{0, 0, 100, 20};
    const Rect left = cut_left(r, 30);
    CHECK(left.x == 0);
    CHECK(left.w == 30);
    CHECK(r.x == 30);
    CHECK(r.w == 70);

    CHECK(cut_left(r, 500).w == 70);
    CHECK(r.w == 0);
    CHECK(cut_left(r, -5).w == 0);

    Rect v{0, 0, 10, 100};
    const Rect bottom = cut_bottom(v, 25);
    CHECK(bottom.y == 75);
    CHECK(bottom.h == 25);
    CHECK(v.h == 75);
    const Rect right = cut_right(v, 4);
    CHECK(right.x == 6);
    CHECK(v.w == 6);
    const Rect top = cut_top(v, 10);
    CHECK(top.y == 0);
    CHECK(v.y == 10);

    const Rect tiny = inset({0, 0, 4, 4}, 10);
    CHECK(tiny.w == 0);
    CHECK(tiny.h == 0);
    CHECK(inset({0, 0, 40, 40}, 5).w == 30);
}

TEST_CASE("rect contains is half-open") {
    const Rect r{10, 10, 20, 20};
    CHECK(r.contains({10, 10}));
    CHECK(r.contains({29.9f, 29.9f}));
    CHECK_FALSE(r.contains({30, 15}));
    CHECK_FALSE(r.contains({9.9f, 15}));
    CHECK(Rect{0, 0, 0, 5}.empty());
}
