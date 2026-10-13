// A UiHost for tests: no window, no GL. Focus and forgetting go to a real
// EventRouter, everything else is recorded.
#pragma once

#include <string>

#include "ui/event_router.h"
#include "ui/widget.h"

namespace aloglas::test {

struct FakeHost : UiHost {
    explicit FakeHost(EventRouter& r) : router(r) {}

    void invalidate() override { ++invalidations; }
    void set_focus(Widget* w) override { router.set_focus(w); }
    Widget* focus() const override { return router.focus(); }
    void forget(Widget& subtree) override { router.forget(subtree); }
    std::string clipboard_text() override { return clipboard; }
    void set_clipboard_text(std::string_view text) override { clipboard = std::string(text); }
    void set_text_input(bool on) override { text_input = on; }

    EventRouter& router;
    int invalidations = 0;
    std::string clipboard;
    bool text_input = false;
};

inline void click(EventRouter& r, Point p, Modifiers mods = {}, int clicks = 1,
                  MouseButton button = MouseButton::Left) {
    MouseEvent e{p, button, clicks, mods};
    r.mouse_down(e);
    r.mouse_up(e);
}

inline void drag(EventRouter& r, Point from, Point to, Modifiers mods = {},
                 MouseButton button = MouseButton::Left) {
    r.mouse_down({from, button, 1, mods});
    r.mouse_move({to, button, 1, mods});
    r.mouse_up({to, button, 1, mods});
}

inline void key(EventRouter& r, Key k, Modifiers mods = {}) {
    r.key({k, mods});
}

inline void type(EventRouter& r, std::string_view utf8) {
    r.text({utf8});
}

}  // namespace aloglas::test
