// Splitter, meter scale and the press-within hook.
#include <doctest.h>

#include "support/fake_host.h"
#include "ui/widgets/button.h"
#include "ui/widgets/meter_bar.h"
#include "ui/widgets/splitter.h"

using namespace aloglas;
using namespace aloglas::test;

namespace {
struct CountingPanel : Widget {
    void on_press_within(const MouseEvent&) override { ++presses; }
    int presses = 0;
};
}  // namespace

TEST_CASE("on_press_within reaches ancestors even when a child handles the press") {
    Widget root;
    CountingPanel panel;
    Button button("OK");
    root.rect = {0, 0, 200, 200};
    root.add(panel);
    panel.add(button);
    panel.layout({0, 0, 100, 100});
    button.layout({10, 10, 40, 20});
    EventRouter router(root);
    FakeHost host(router);
    root.set_host(&host);
    int clicks = 0;
    button.on_click = [&] { ++clicks; };

    click(router, {20, 20});
    CHECK(panel.presses == 1);
    CHECK(clicks == 1);

    click(router, {80, 80});  // the panel itself, no child under the cursor
    CHECK(panel.presses == 2);
    click(router, {150, 150});  // outside the panel
    CHECK(panel.presses == 2);
}

TEST_CASE("splitter drags with sign and resets on double-click") {
    Widget root;
    root.rect = {0, 0, 1000, 500};
    float value = 220;
    Splitter split(SplitAxis::Columns, [&] { return value; }, [&](float v) { value = v; }, +1, [&] { value = 220; });
    root.add(split);
    split.layout({218, 0, 4, 500});
    EventRouter router(root);
    FakeHost host(router);
    root.set_host(&host);

    router.mouse_move({{220, 100}});
    CHECK(host.cursor == Cursor::ResizeH);
    drag(router, {220, 100}, {280, 300});
    CHECK(value == 280);
    router.mouse_move({{500, 100}});
    CHECK(host.cursor == Cursor::Arrow);

    click(router, {220, 100}, {}, 2);
    CHECK(value == 220);

    Splitter rows(SplitAxis::Rows, [&] { return value; }, [&](float v) { value = v; }, -1, nullptr);
    root.add(rows);
    rows.layout({0, 398, 1000, 4});
    drag(router, {500, 400}, {700, 350});  // up 50 px with sign -1 → +50
    CHECK(value == 270);
}

TEST_CASE("meter_fraction maps dB") {
    CHECK(meter_fraction(0.0f) == 0.0f);
    CHECK(meter_fraction(1.0f) == doctest::Approx(60.0f / 66.0f));
    CHECK(meter_fraction(4.0f) == 1.0f);  // +12 dB clamps
    CHECK(meter_fraction(0.0005f) == 0.0f);  // below -60 dB
}
