// Basic widgets driven through the EventRouter, without a window or drawing.
#include <doctest.h>

#include <string>
#include <vector>

#include "support/fake_host.h"
#include "ui/widgets/button.h"
#include "ui/widgets/number_box.h"
#include "ui/widgets/text_field.h"
#include "ui/widgets/toggle_button.h"

using namespace aloglas;
using namespace aloglas::test;

namespace {
// Root that records the keys reaching it (i.e. not consumed by a child).
struct Root : Widget {
    bool on_key(const KeyEvent& e) override {
        keys.push_back(e.key);
        return true;
    }
    std::vector<Key> keys;
};

// Root (0,0,400,200), one child widget at (10,10,100,24), and an empty area.
template <typename W>
struct Fixture {
    Root root;
    W widget;
    EventRouter router{root};
    FakeHost host{router};

    template <typename... Args>
    explicit Fixture(Args&&... args) : widget(std::forward<Args>(args)...) {
        root.rect = {0, 0, 400, 200};
        root.add(widget);
        widget.layout({10, 10, 100, 24});
        root.set_host(&host);
    }
    static constexpr Point inside{50, 20};
    static constexpr Point outside{300, 150};
};
}  // namespace

TEST_CASE("Button clicks only when released inside") {
    Fixture<Button> f("OK");
    int clicks = 0;
    f.widget.on_click = [&] { ++clicks; };
    click(f.router, f.inside);
    CHECK(clicks == 1);

    f.router.mouse_down({f.inside});
    f.router.mouse_move({f.outside});
    f.router.mouse_up({f.outside});
    CHECK(clicks == 1);
}

TEST_CASE("ToggleButton flips the value it reads") {
    Fixture<ToggleButton> f("Metro");
    bool on = false;
    f.widget.value = [&] { return on; };
    f.widget.on_toggle = [&](bool v) { on = v; };
    click(f.router, f.inside);
    CHECK(on);
    click(f.router, f.inside);
    CHECK_FALSE(on);
}

TEST_CASE("TextField commit, cancel, focus loss") {
    Fixture<TextField> f;
    std::vector<std::string> commits;
    int cancels = 0;
    f.widget.on_commit = [&](const std::string& s) { commits.push_back(s); };
    f.widget.on_cancel = [&] { ++cancels; };

    SUBCASE("Enter commits once") {
        f.widget.begin("old");
        CHECK(f.router.focus() == &f.widget);
        CHECK(f.host.text_input);
        type(f.router, "new");             // replaces the selected "old"
        key(f.router, Key::Enter);
        CHECK(commits == std::vector<std::string>{"new"});
        CHECK_FALSE(f.widget.active());
        CHECK(f.router.focus() == nullptr);
        CHECK_FALSE(f.host.text_input);
        f.router.set_focus(nullptr);       // nothing more to commit
        CHECK(commits.size() == 1);
    }
    SUBCASE("Escape cancels") {
        f.widget.begin("old");
        type(f.router, "x");
        key(f.router, Key::Escape);
        CHECK(commits.empty());
        CHECK(cancels == 1);
        CHECK(f.router.focus() == nullptr);
    }
    SUBCASE("clicking elsewhere commits the current text") {
        f.widget.begin("old");
        key(f.router, Key::End);
        type(f.router, "er");
        click(f.router, f.outside);
        CHECK(commits == std::vector<std::string>{"older"});
    }
}

// Final review finding 7: the committed text must not change under the
// callback when the callback edits (or rebuilds) the field.
TEST_CASE("TextField commit on focus loss passes a stable copy") {
    Fixture<TextField> f;
    std::string seen;
    f.widget.on_commit = [&](const std::string& s) {
        f.widget.edit().set_text("changed");
        seen = s;
    };
    f.widget.begin("old");
    click(f.router, f.outside);
    CHECK(seen == "old");
}

TEST_CASE("TextField consumes Space and Delete, lets Ctrl+T bubble") {
    Fixture<TextField> f;
    f.widget.begin("a b");
    key(f.router, Key::Space);
    key(f.router, Key::Delete);
    key(f.router, Key::Up);
    CHECK(f.root.keys.empty());
    key(f.router, Key::T, {.ctrl = true});
    CHECK(f.root.keys == std::vector<Key>{Key::T});
}

TEST_CASE("TextField clipboard") {
    Fixture<TextField> f;
    f.widget.begin("Kick");
    key(f.router, Key::C, {.ctrl = true});
    CHECK(f.host.clipboard == "Kick");
    key(f.router, Key::End);
    key(f.router, Key::V, {.ctrl = true});
    CHECK(f.widget.edit().text() == "KickKick");
}

TEST_CASE("NumberBox drag") {
    Fixture<NumberBox> f(NumberSpec{20, 999, 2, 0.5, 0.01, {}});
    double value = 140;
    std::vector<double> changes;
    f.widget.value = [&] { return value; };
    f.widget.on_change = [&](double v) { changes.push_back(v); value = v; };

    SUBCASE("coarse") {
        drag(f.router, f.inside, {f.inside.x, f.inside.y - 20});
        REQUIRE_FALSE(changes.empty());
        CHECK(changes.back() == doctest::Approx(150));
    }
    SUBCASE("fine with shift") {
        drag(f.router, f.inside, {f.inside.x, f.inside.y - 20}, {.shift = true});
        REQUIRE_FALSE(changes.empty());
        CHECK(changes.back() == doctest::Approx(140.2));
    }
    SUBCASE("a click without movement changes nothing") {
        click(f.router, f.inside);
        CHECK(changes.empty());
    }
}

TEST_CASE("NumberBox text entry") {  // Review Focus 3
    Fixture<NumberBox> f(NumberSpec{20, 999, 2, 0.5, 0.01, {}});
    double value = 140;
    std::vector<double> changes;
    f.widget.value = [&] { return value; };
    f.widget.on_change = [&](double v) { changes.push_back(v); value = v; };

    auto start_typing = [&] {
        click(f.router, f.inside, {}, 1);
        click(f.router, f.inside, {}, 2);  // double-click opens the text entry
    };

    SUBCASE("comma decimal") {
        start_typing();
        type(f.router, "140,5");
        key(f.router, Key::Enter);
        CHECK(changes == std::vector<double>{140.5});
    }
    SUBCASE("garbage keeps the old value") {
        start_typing();
        type(f.router, "abc");
        key(f.router, Key::Enter);
        CHECK(changes.empty());
    }
    SUBCASE("escape keeps the old value") {
        start_typing();
        type(f.router, "90");
        key(f.router, Key::Escape);
        CHECK(changes.empty());
    }
    SUBCASE("out of range is clamped") {
        start_typing();
        type(f.router, "5000");
        key(f.router, Key::Enter);
        CHECK(changes == std::vector<double>{999});
    }
    SUBCASE("after editing, dragging works again") {
        start_typing();
        key(f.router, Key::Escape);
        drag(f.router, f.inside, {f.inside.x, f.inside.y - 20});
        REQUIRE_FALSE(changes.empty());
        CHECK(changes.back() == doctest::Approx(150));
    }
}
