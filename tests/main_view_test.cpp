#include <doctest.h>

#include "app/browser_model.h"
#include "app/browser_panel.h"
#include "app/controller.h"
#include "app/previewer.h"
#include "app/log.h"
#include "app/main_view.h"
#include "samples/sample_cache.h"
#include "samples/sample_loader.h"
#include "support/fake_host.h"
#include "support/recording_sink.h"

using namespace aloglas;
using namespace aloglas::test;

namespace {
Point center(Rect r) {
    return {r.x + r.w / 2, r.y + r.h / 2};
}

struct Fixture {
    Document doc;
    RecordingSink sink;
    Controller controller{doc, sink};
    LiveState live;
    Log log;
    SampleLoader loader;
    SampleCache cache;
    Previewer previewer{sink, loader, cache, log};
    BrowserModel model{log};
    PopupMenu popup;
    BrowserPanel browser{model, previewer, popup};
    MainView view{controller, live, log, browser, popup};
    EventRouter router{view};
    FakeHost host{router};

    Fixture() {
        controller.ensure_audio = [] { return true; };
        view.set_host(&host);
        view.layout({0, 0, 1280, 760});
    }
};

bool all_sizes_valid(const Widget& w) {
    if (w.rect.w < 0 || w.rect.h < 0) return false;
    for (const Widget* c : w.children()) {
        if (!all_sizes_valid(*c)) return false;
    }
    return true;
}
}  // namespace

TEST_CASE("MainView shortcuts") {
    Fixture f;
    key(f.router, Key::Space);
    REQUIRE_FALSE(f.sink.sent.empty());
    CHECK(f.sink.sent.back().type == CommandType::Play);
    f.live.playing = true;
    key(f.router, Key::Space);
    CHECK(f.sink.sent.back().type == CommandType::Stop);
    key(f.router, Key::L, {.ctrl = true});
    CHECK(f.doc.brace.enabled);
}

TEST_CASE("Space while typing a tempo edits the text") {
    Fixture f;
    click(f.router, center(f.view.transport().tempo().rect), {}, 2);
    f.sink.sent.clear();
    key(f.router, Key::Space);
    CHECK(f.sink.sent.empty());
}

TEST_CASE("clicking inside a panel focuses it") {
    Fixture f;
    CHECK(f.view.focused_panel() == nullptr);
    click(f.router, center(f.view.main_layout().log));
    REQUIRE(f.view.focused_panel() != nullptr);
    CHECK(f.view.focused_panel()->title() == "LOG");
    click(f.router, center(f.view.main_layout().transport));
    CHECK(f.view.focused_panel()->title() == "LOG");
    click(f.router, center(f.view.main_layout().piano));
    CHECK(f.view.focused_panel()->title() == "PIANO ROLL");
}

TEST_CASE("splitter drag clamps and double-click resets") {
    Fixture f;
    const Point b = center(f.view.main_layout().split_browser);
    drag(f.router, b, {b.x + 2000, b.y});
    CHECK(f.view.main_layout().playlist.w >= 240 - 1);
    CHECK(f.view.sizes().browser_width > 220);
    const Point b2 = center(f.view.main_layout().split_browser);
    click(f.router, b2, {}, 2);
    CHECK(f.view.sizes().browser_width == 220);

    const Point p = center(f.view.main_layout().split_top_piano);
    drag(f.router, p, {p.x, p.y - 1000});
    CHECK(f.view.main_layout().playlist.h >= 120 - 1);

    const Point r = center(f.view.main_layout().split_rack_log);
    drag(f.router, r, {r.x, r.y + 1000});
    CHECK(f.view.main_layout().log.h >= 60 - 1);
    click(f.router, center(f.view.main_layout().split_top_piano), {}, 2);  // room for the default again
    CHECK(f.view.sizes().piano_share == doctest::Approx(0.4f));
    const Point r2 = center(f.view.main_layout().split_rack_log);
    click(f.router, r2, {}, 2);
    CHECK(f.view.sizes().rack_share == doctest::Approx(0.6f));
}

// Final review: the splitter read the stored size while the layout showed a
// clamped one, so it did nothing until the mouse had covered the difference.
TEST_CASE("splitter follows the mouse when the saved size does not fit") {
    Fixture f;
    f.view.sizes().right_width = 900;  // saved on a wider monitor; only 819 fits
    f.view.layout({0, 0, 1280, 760});
    const float rack_x = f.view.main_layout().rack.x;
    const Point p = center(f.view.main_layout().split_right);
    drag(f.router, p, {p.x + 40, p.y});
    CHECK(f.view.main_layout().rack.x == doctest::Approx(rack_x + 40));
}

TEST_CASE("MainView layout survives tiny sizes") {
    Fixture f;
    for (Rect r : {Rect{0, 0, 0, 0}, Rect{0, 0, 120, 60}, Rect{0, 0, 1280, 760}}) {
        f.view.layout(r);
        CHECK(all_sizes_valid(f.view));
    }
    CHECK(f.view.sizes() == PanelSizes{});  // a tiny window doesn't destroy the saved sizes
}
