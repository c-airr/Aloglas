#include <doctest.h>

#include <initializer_list>

#include "app/main_layout.h"
#include "ui/theme.h"

using namespace aloglas;

namespace {
bool inside(Rect r, Rect outer) {
    return r.w >= 0 && r.h >= 0 && r.x >= outer.x && r.y >= outer.y && r.right() <= outer.right() + 0.01f &&
           r.bottom() <= outer.bottom() + 0.01f;
}
}  // namespace

TEST_CASE("compute_layout at 1280x760 matches the spec defaults") {
    const MainLayout l = compute_layout({0, 0, 1280, 760}, {}, theme());
    CHECK(l.transport.h == 28);
    CHECK(l.status.h == 18);
    CHECK(l.status.bottom() == 760);
    CHECK(l.browser.w == doctest::Approx(219));  // 220 minus the 1-px gap
    CHECK(l.browser.y == 28);
    CHECK(l.browser.bottom() == doctest::Approx(760 - 18));
    CHECK(l.rack.y == l.playlist.y);
    CHECK(l.rack.right() == 1280);
    CHECK(l.rack.x == doctest::Approx(1280 - 280));
    CHECK(l.piano.x == l.playlist.x);
    CHECK(l.piano.right() == 1280);
    CHECK(l.piano.h == doctest::Approx((760 - 28 - 18) * 0.4f).epsilon(0.01));
    CHECK(l.log.bottom() == doctest::Approx(l.playlist.bottom()));
    // splitters are centred on the gaps
    CHECK(l.split_browser.x + l.split_browser.w / 2 == doctest::Approx(219.5f));
    CHECK(l.split_browser.w == theme().splitter);
    CHECK(l.split_top_piano.h == theme().splitter);
}

TEST_CASE("compute_layout survives tiny windows") {
    for (Rect w : {Rect{0, 0, 0, 0}, Rect{0, 0, 50, 30}, Rect{0, 0, 960, 600}}) {
        CAPTURE(w.w);
        const MainLayout l = compute_layout(w, {}, theme());
        for (Rect r : {l.transport, l.status, l.browser, l.playlist, l.rack, l.log, l.piano}) CHECK(inside(r, w));
    }
}

TEST_CASE("clamp_sizes keeps neighbours at their minimum") {
    const Rect w{0, 0, 1280, 760};
    PanelSizes s;
    s.browser_width = 5000;
    CHECK(clamp_sizes(s, w, theme()).browser_width == 1280 - (240 + 180 + 1));
    s.browser_width = -50;
    CHECK(clamp_sizes(s, w, theme()).browser_width == 140);
    s = {};
    s.right_width = 5000;
    CHECK(clamp_sizes(s, w, theme()).right_width == 1280 - 220 - 240 - 1);
    s = {};
    s.rack_share = 2;
    s.piano_share = -1;
    const PanelSizes c = clamp_sizes(s, w, theme());
    CHECK(c.rack_share <= 1.0f);
    CHECK(c.piano_share >= 0.0f);
    const MainLayout l = compute_layout(w, c, theme());
    CHECK(l.log.h >= 60 - 1);
    CHECK(l.piano.h >= 120 - 1);
    CHECK(clamp_sizes({}, w, theme()) == PanelSizes{});  // defaults already fit
    const PanelSizes tiny = clamp_sizes({}, {0, 0, 50, 30}, theme());
    CHECK(tiny.browser_width >= 0);
    CHECK(tiny.right_width >= 0);
}
