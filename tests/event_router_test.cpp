#include <doctest.h>

#include <functional>
#include <string>
#include <vector>

#include "support/fake_host.h"
#include "ui/event_router.h"
#include "ui/widget.h"

using namespace aloglas;
using namespace aloglas::test;

namespace {
// Records every callback into a shared log, e.g. "a:down", "b:focus_lost".
struct Probe : Widget {
    Probe(std::string n, std::vector<std::string>& log) : name(std::move(n)), log_(log) {}

    bool on_mouse_down(const MouseEvent&) override { log_.push_back(name + ":down"); return handles_mouse; }
    void on_mouse_up(const MouseEvent&) override { log_.push_back(name + ":up"); }
    void on_mouse_move(const MouseEvent&) override { log_.push_back(name + ":move"); }
    bool on_wheel(const WheelEvent&) override { log_.push_back(name + ":wheel"); return handles_wheel; }
    bool on_key(const KeyEvent&) override { log_.push_back(name + ":key"); return handles_keys; }
    bool on_text(const TextEvent& e) override { log_.push_back(name + ":text:" + std::string(e.utf8)); return true; }
    void on_hover_enter() override { log_.push_back(name + ":enter"); }
    void on_hover_leave() override { log_.push_back(name + ":leave"); }
    void on_focus_gained() override { log_.push_back(name + ":focus_gained"); }
    void on_focus_lost() override {
        log_.push_back(name + ":focus_lost");
        if (on_lost) on_lost();
    }
    bool focusable() const override { return can_focus; }

    std::string name;
    bool handles_mouse = false;
    bool handles_wheel = false;
    bool handles_keys = false;
    bool can_focus = false;
    std::function<void()> on_lost;  // runs inside on_focus_lost (e.g. a commit that rebuilds rows)

private:
    std::vector<std::string>& log_;
};

// root (0,0,100,100) with a (0,0,50,50), b (25,25,50,50) added after a,
// and a1 (0,0,10,10) inside a.
struct Tree {
    std::vector<std::string> log;
    Probe root{"root", log}, a{"a", log}, b{"b", log}, a1{"a1", log};
    EventRouter router{root};
    FakeHost host{router};

    Tree() {
        root.rect = {0, 0, 100, 100};
        a.rect = {0, 0, 50, 50};
        b.rect = {25, 25, 50, 50};
        a1.rect = {0, 0, 10, 10};
        root.add(a);
        root.add(b);
        a.add(a1);
        root.set_host(&host);
    }
};
}  // namespace

TEST_CASE("hit_test picks the deepest, topmost visible widget") {
    Tree t;
    CHECK(hit_test(t.root, {30, 30}) == &t.b);   // b was added later: it is on top
    CHECK(hit_test(t.root, {5, 5}) == &t.a1);
    CHECK(hit_test(t.root, {15, 5}) == &t.a);
    CHECK(hit_test(t.root, {90, 10}) == &t.root);
    CHECK(hit_test(t.root, {200, 10}) == nullptr);

    t.b.visible = false;
    CHECK(hit_test(t.root, {30, 30}) == &t.a);
    t.a1.enabled = false;
    CHECK(hit_test(t.root, {5, 5}) == &t.a);
}

TEST_CASE("mouse_down bubbles until handled and captures") {
    Tree t;
    t.a.handles_mouse = true;
    t.router.mouse_down({{5, 5}});
    CHECK(t.log == std::vector<std::string>{"a1:down", "a:down"});
    CHECK(t.router.captured() == &t.a);

    t.log.clear();
    t.router.mouse_move({{90, 90}});   // far outside a: still delivered to a
    t.router.mouse_up({{90, 90}});
    CHECK(t.log.size() >= 2);
    CHECK(t.log[0] == "a:move");
    CHECK(t.log[1] == "a:up");
    CHECK(t.router.captured() == nullptr);
}

TEST_CASE("clicking outside the focused widget removes focus before delivery") {
    Tree t;
    t.a.can_focus = true;
    t.b.handles_mouse = true;
    t.router.set_focus(&t.a);
    CHECK(t.router.focus() == &t.a);
    CHECK(t.a.has_focus());

    t.log.clear();
    t.router.mouse_down({{60, 60}});   // inside b, outside a
    REQUIRE(t.log.size() >= 2);
    CHECK(t.log[0] == "a:focus_lost");
    CHECK(t.log[1] == "b:down");
    CHECK(t.router.focus() == nullptr);
    t.router.mouse_up({{60, 60}});

    // Clicking inside the focused widget (or its child) keeps focus.
    t.router.set_focus(&t.a);
    t.log.clear();
    t.router.mouse_down({{5, 5}});
    CHECK(t.router.focus() == &t.a);
    t.router.mouse_up({{5, 5}});
}

TEST_CASE("set_focus ignores widgets that are not focusable") {
    Tree t;
    t.router.set_focus(&t.b);
    CHECK(t.router.focus() == nullptr);
}

TEST_CASE("hover enter and leave") {
    Tree t;
    t.router.mouse_move({{15, 5}});
    CHECK(t.router.hovered() == &t.a);
    t.router.mouse_move({{60, 60}});
    CHECK(t.router.hovered() == &t.b);
    t.router.mouse_left_window();
    CHECK(t.router.hovered() == nullptr);
    CHECK(t.log == std::vector<std::string>{"a:enter", "a:move", "a:leave", "b:enter", "b:move", "b:leave"});
}

TEST_CASE("keys go to focus, bubble to root, and go to root when nothing has focus") {
    Tree t;
    t.root.handles_keys = true;
    t.a1.can_focus = true;
    t.router.set_focus(&t.a1);
    t.log.clear();
    CHECK(t.router.key({Key::Space}));
    CHECK(t.log == std::vector<std::string>{"a1:key", "a:key", "root:key"});

    t.router.set_focus(nullptr);
    t.log.clear();
    CHECK(t.router.key({Key::Space}));
    CHECK(t.log == std::vector<std::string>{"root:key"});
}

TEST_CASE("wheel bubbles from the widget under the mouse") {
    Tree t;
    t.a.handles_wheel = true;
    CHECK(t.router.wheel({{5, 5}, 0, 1}));
    CHECK(t.log == std::vector<std::string>{"a1:wheel", "a:wheel"});
}

TEST_CASE("text goes only to the focused widget") {
    Tree t;
    CHECK_FALSE(t.router.text({"x"}));
    CHECK(t.log.empty());
    t.b.can_focus = true;
    t.router.set_focus(&t.b);
    t.log.clear();
    CHECK(t.router.text({"ż"}));
    CHECK(t.log == std::vector<std::string>{"b:text:ż"});
}

TEST_CASE("forget clears pointers into a removed subtree without callbacks") {
    Tree t;
    t.a1.can_focus = true;
    t.a1.handles_mouse = true;
    t.router.set_focus(&t.a1);
    t.router.mouse_down({{5, 5}});
    t.router.mouse_move({{5, 5}});
    REQUIRE(t.router.captured() == &t.a1);
    t.log.clear();

    t.root.remove(t.a);   // goes through FakeHost::forget
    CHECK(t.router.focus() == nullptr);
    CHECK(t.router.captured() == nullptr);
    CHECK(t.router.hovered() == nullptr);
    CHECK(t.log.empty());  // no focus_lost / leave callbacks into the removed widgets
    CHECK(t.a.parent() == nullptr);
    CHECK(t.root.children().size() == 1);
}

TEST_CASE("widget invalidate reaches the host") {
    Tree t;
    t.a1.invalidate();
    CHECK(t.host.invalidations == 1);
}

// Final review finding 1: a focus-loss commit can change the widget tree
// before the click that caused it is delivered.
TEST_CASE("a widget removed while focus is lost does not receive the click") {
    Tree t;
    t.a.can_focus = true;
    t.b.handles_mouse = true;
    t.root.handles_mouse = true;
    t.router.set_focus(&t.a);
    t.a.on_lost = [&] { t.root.remove(t.b); };
    t.log.clear();
    t.router.mouse_down({{60, 60}});  // inside b, outside a
    CHECK(t.log == std::vector<std::string>{"a:focus_lost", "root:down"});
    CHECK(t.router.captured() == &t.root);
}

TEST_CASE("focus_gained is not delivered to a widget removed during focus_lost") {
    Tree t;
    t.a.can_focus = true;
    t.b.can_focus = true;
    t.router.set_focus(&t.a);
    t.a.on_lost = [&] { t.root.remove(t.b); };
    t.log.clear();
    t.router.set_focus(&t.b);
    CHECK(t.log == std::vector<std::string>{"a:focus_lost"});
    CHECK(t.router.focus() == nullptr);
}

// Final review finding 5.
TEST_CASE("a second mouse button neither steals nor releases the capture") {
    Tree t;
    t.a.handles_mouse = true;
    t.b.handles_mouse = true;
    t.router.mouse_down({{5, 5}, MouseButton::Left});
    REQUIRE(t.router.captured() == &t.a);
    t.router.mouse_down({{60, 60}, MouseButton::Right});
    t.router.mouse_up({{60, 60}, MouseButton::Right});
    CHECK(t.router.captured() == &t.a);
    t.log.clear();
    t.router.mouse_move({{90, 90}});
    t.router.mouse_up({{90, 90}, MouseButton::Left});
    REQUIRE(t.log.size() >= 2);
    CHECK(t.log[0] == "a:move");
    CHECK(t.log[1] == "a:up");
    CHECK(t.router.captured() == nullptr);
}
