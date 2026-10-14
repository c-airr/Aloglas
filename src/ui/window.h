// The application window: SDL3 window + OpenGL 3.3 context + NanoVG.
// Owns the event router, turns SDL events into toolkit events, and draws the
// widget tree only when something changed (or while animating).
#pragma once

#include <functional>
#include <memory>
#include <string>

#include "ui/event_router.h"
#include "ui/widget.h"

struct SDL_Window;
struct SDL_GLContextState;
union SDL_Event;
struct NVGcontext;

namespace aloglas {

struct WindowOptions {
    std::string title = "Aloglas";
    int width = 1280;        // size when not maximized
    int height = 760;
    int min_width = 960;
    int min_height = 600;
    bool maximized = true;   // first start: fill the screen (normal window, not fullscreen)
    bool hidden = false;     // offscreen rendering (screenshots)
};

class Window final : public UiHost {
public:
    explicit Window(const WindowOptions& options);
    ~Window() override;
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool ok() const { return ok_; }
    const std::string& error() const { return error_; }

    void set_root(Widget& root);

    // Runs until the window is closed. `before_frame` runs once per loop turn
    // (after input); returning true asks for continuous frames (e.g. while
    // the transport plays). When nothing changes the loop sleeps in
    // SDL_WaitEvent and uses no CPU.
    void run(const std::function<bool()>& before_frame);

    // Renders one frame of the root into an offscreen buffer at scale 1 and
    // saves it as PNG. Works with a hidden window.
    bool render_to_png(const std::string& path, int width, int height);

    // UiHost
    void invalidate() override { dirty_ = true; }
    void set_focus(Widget* w) override;
    Widget* focus() const override;
    void forget(Widget& subtree) override;
    std::string clipboard_text() override;
    void set_clipboard_text(std::string_view text) override;
    void set_text_input(bool on) override;

private:
    void handle(const SDL_Event& e, bool& running);
    void render_frame();
    void draw_root(float width, float height, float pixel_ratio);
    float to_logical() const;  // window coordinates -> logical pixels factor
    float ui_scale() const;    // logical pixels -> framebuffer pixels

    SDL_Window* window_ = nullptr;
    SDL_GLContextState* gl_ = nullptr;
    NVGcontext* vg_ = nullptr;
    int font_regular_ = -1;
    int font_bold_ = -1;
    Widget* root_ = nullptr;
    std::unique_ptr<EventRouter> router_;
    bool dirty_ = true;
    bool ok_ = false;
    std::string error_;
};

}  // namespace aloglas
