// Input events in toolkit terms (the Window translates SDL events into these),
// so widgets and tests never depend on SDL.
#pragma once

#include <string_view>

#include "ui/geometry.h"

namespace aloglas {

enum class Key { Unknown, Space, Enter, Escape, Backspace, Delete, Left, Right, Up, Down, Home, End, Tab, A, C, L, T, V, X };

struct Modifiers {
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
};

enum class MouseButton { Left, Right, Middle };

struct MouseEvent {
    Point pos;
    MouseButton button = MouseButton::Left;
    int clicks = 1;  // 2 on the second press of a double-click
    Modifiers mods;
};

struct WheelEvent {
    Point pos;
    float dx = 0;
    float dy = 0;  // > 0: wheel rolled away from the user
    Modifiers mods;
};

struct KeyEvent {
    Key key = Key::Unknown;
    Modifiers mods;
    bool repeat = false;
};

// Text typed by the user (already composed by the OS: Polish letters, IME).
// Only valid during the call.
struct TextEvent {
    std::string_view utf8;
};

}  // namespace aloglas
