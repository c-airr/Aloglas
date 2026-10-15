// The application window: SDL3 window + OpenGL 3.3 context + NanoVG.
// Owns the event router, turns SDL events into toolkit events, and draws the
// widget tree only when something changed (or while animating).
#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "ui/event_router.h"
#include "ui/widget.h"

struct SDL_Window;
struct SDL_GLContextState;
struct SDL_Cursor;
union SDL_Event;
struct NVGcontext;

namespace aloglas {

// Where the window is when it is not maximized (screen coordinates), and
// whether it is maximized. Saved in settings between runs.
struct WindowPlacement {
    int x = 0;
    int y = 0;
    int w = 1280;
    int h = 760;
    bool maximized = true;
};

// True when enough of `window` lies on one of `displays` to grab and move it
// (a saved position on a monitor that is now unplugged is not).
bool mostly_visible(Rect window, const std::vector<Rect>& displays);

// Shrinks the size (not the position) so the window fits on `display` with
// room for its title bar.
WindowPlacement fit_to_display(WindowPlacement p, Rect display);

struct WindowOptions {
    std::string title = "Aloglas";
    int width = 1280;        // size when not maximized
    int height = 760;
    int min_width = 960;
    int min_height = 600;
    bool maximized = true;   // first start: fill the screen (normal window, not fullscreen)
    bool hidden = false;     // offscreen rendering (screenshots)
    // Last run's placement; replaces width/height/maximized. Ignored when it
    // is not on any current display (the window is then centred).
    std::optional<WindowPlacement> placement;
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

    // The non-maximized position and size (even while maximized), plus
    // whether the window is maximized now.
    WindowPlacement placement() const;

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
    void set_cursor(Cursor c) override;
    void pick_folder(std::function<void(std::filesystem::path)> on_picked) override;

    // Thread-safe: makes run() wake up from its idle wait (a background
    // thread finished something the UI should show).
    void wake();

private:
    void handle(const SDL_Event& e, bool& running);
    void render_frame();
    void draw_root(float width, float height, float pixel_ratio);
    float to_logical() const;  // window coordinates -> logical pixels factor
    float ui_scale() const;    // logical pixels -> framebuffer pixels
    void remember_normal_placement();
    void deliver_folder();  // UI thread: hands a picked folder to its callback
    friend struct FolderDialog;

    SDL_Window* window_ = nullptr;
    SDL_GLContextState* gl_ = nullptr;
    NVGcontext* vg_ = nullptr;
    int font_regular_ = -1;
    int font_bold_ = -1;
    SDL_Cursor* cursors_[3] = {};  // indexed by Cursor, created on first use
    Widget* root_ = nullptr;
    std::unique_ptr<EventRouter> router_;
    bool dirty_ = true;
    uint32_t wake_event_ = 0;  // SDL user event type
    // Folder dialog: SDL may call back on another thread, so the result
    // waits here under a mutex until the UI thread picks it up.
    std::function<void(std::filesystem::path)> folder_callback_;
    bool folder_dialog_open_ = false;
    std::mutex folder_mutex_;
    bool folder_done_ = false;
    std::optional<std::filesystem::path> folder_result_;
    WindowPlacement normal_;  // last non-maximized, non-minimized position and size
    bool ok_ = false;
    std::string error_;
};

}  // namespace aloglas
