#include "ui/window.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include <SDL3/SDL.h>
#include <glad/gl.h>  // must come before NanoVG's GL backend
#include <nanovg.h>
#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg_gl.h>
#include <nanovg_gl_utils.h>

#include "ui/canvas.h"
#include "ui/fonts.h"
#include "ui/theme.h"

namespace aloglas {

void use_dark_title_bar(void* hwnd);  // win32_titlebar.cpp

namespace {

Key map_key(SDL_Keycode k) {
    switch (k) {
    case SDLK_SPACE: return Key::Space;
    case SDLK_RETURN:
    case SDLK_KP_ENTER: return Key::Enter;
    case SDLK_ESCAPE: return Key::Escape;
    case SDLK_BACKSPACE: return Key::Backspace;
    case SDLK_DELETE: return Key::Delete;
    case SDLK_LEFT: return Key::Left;
    case SDLK_RIGHT: return Key::Right;
    case SDLK_UP: return Key::Up;
    case SDLK_DOWN: return Key::Down;
    case SDLK_HOME: return Key::Home;
    case SDLK_END: return Key::End;
    case SDLK_TAB: return Key::Tab;
    case SDLK_A: return Key::A;
    case SDLK_C: return Key::C;
    case SDLK_L: return Key::L;
    case SDLK_T: return Key::T;
    case SDLK_V: return Key::V;
    case SDLK_X: return Key::X;
    case SDLK_1: return Key::D1;
    case SDLK_2: return Key::D2;
    case SDLK_3: return Key::D3;
    case SDLK_4: return Key::D4;
    default: return Key::Unknown;
    }
}

Modifiers map_mods(SDL_Keymod m) {
    return {(m & SDL_KMOD_SHIFT) != 0, (m & SDL_KMOD_CTRL) != 0, (m & SDL_KMOD_ALT) != 0};
}

bool map_button(Uint8 b, MouseButton& out) {
    switch (b) {
    case SDL_BUTTON_LEFT: out = MouseButton::Left; return true;
    case SDL_BUTTON_RIGHT: out = MouseButton::Right; return true;
    case SDL_BUTTON_MIDDLE: out = MouseButton::Middle; return true;
    default: return false;  // side buttons are ignored
    }
}

void clear_to_background() {
    const Color bg = theme().background;
    glClearColor(bg.r / 255.0f, bg.g / 255.0f, bg.b / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

}  // namespace

Window::Window(const WindowOptions& options) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        error_ = std::string("SDL_Init: ") + SDL_GetError();
        return;
    }
    // NanoVG's GL3 backend needs a 3.3 core context and a stencil buffer
    // (it fills concave shapes with the stencil trick).
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    // Restore the last placement only if it is still on a connected display.
    std::vector<Rect> displays;
    int display_count = 0;
    if (SDL_DisplayID* ids = SDL_GetDisplays(&display_count)) {
        for (int i = 0; i < display_count; ++i) {
            SDL_Rect b;
            if (SDL_GetDisplayUsableBounds(ids[i], &b)) {
                displays.push_back({float(b.x), float(b.y), float(b.w), float(b.h)});
            }
        }
        SDL_free(ids);
    }
    bool maximized = options.maximized;
    int x = SDL_WINDOWPOS_CENTERED;
    int y = SDL_WINDOWPOS_CENTERED;
    normal_ = {0, 0, options.width, options.height, options.maximized};
    if (options.placement) {
        const WindowPlacement& p = *options.placement;
        normal_.w = p.w;
        normal_.h = p.h;
        maximized = p.maximized;
        if (mostly_visible({float(p.x), float(p.y), float(p.w), float(p.h)}, displays)) {
            x = p.x;
            y = p.y;
        } else if (!displays.empty()) {
            // Its monitor is gone: centre it on the main one, at a size that fits.
            const WindowPlacement fitted = fit_to_display(p, displays.front());
            normal_.w = fitted.w;
            normal_.h = fitted.h;
        }
    }

    SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (options.hidden) flags |= SDL_WINDOW_HIDDEN;
    else if (maximized) flags |= SDL_WINDOW_MAXIMIZED;

    // Created at the normal rect and then maximized, so un-maximizing goes
    // back to that rect.
    const SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, options.title.c_str());
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, x);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, y);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, normal_.w);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, normal_.h);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER, flags);
    window_ = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);
    if (!window_) {
        error_ = std::string("SDL_CreateWindow: ") + SDL_GetError();
        return;
    }
    SDL_SetWindowMinimumSize(window_, options.min_width, options.min_height);
    wake_event_ = SDL_RegisterEvents(1);
    if (maximized) {
        // The real normal position is the centred one SDL chose (or the
        // restored one); SDL reports the maximized rect now, so compute it.
        if (x == SDL_WINDOWPOS_CENTERED && !displays.empty()) {
            const Rect d = displays.front();
            normal_.x = int(d.x + (d.w - normal_.w) / 2);
            normal_.y = int(d.y + (d.h - normal_.h) / 2);
        } else {
            normal_.x = x;
            normal_.y = y;
        }
    } else {
        remember_normal_placement();
    }
    use_dark_title_bar(SDL_GetPointerProperty(SDL_GetWindowProperties(window_),
                                              SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));

    gl_ = SDL_GL_CreateContext(window_);
    if (!gl_) {
        error_ = std::string("SDL_GL_CreateContext: ") + SDL_GetError();
        return;
    }
    SDL_GL_MakeCurrent(window_, gl_);
    // Windows' opengl32.dll only exports OpenGL 1.1; glad looks up every
    // newer function through the driver at runtime.
    if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress))) {
        error_ = "failed to load OpenGL 3.3 functions";
        return;
    }
    SDL_GL_SetSwapInterval(1);  // vsync: at most one frame per display refresh

    vg_ = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    if (!vg_) {
        error_ = "failed to create the NanoVG context";
        return;
    }
    // freeData = 0: the font bytes live in the executable and must not be freed.
    // NanoVG takes a non-const pointer but only reads the data.
    const auto regular = inter_regular();
    const auto bold = inter_semibold();
    font_regular_ = nvgCreateFontMem(vg_, "inter", const_cast<unsigned char*>(regular.data()),
                                     static_cast<int>(regular.size()), 0);
    font_bold_ = nvgCreateFontMem(vg_, "inter-semibold", const_cast<unsigned char*>(bold.data()),
                                  static_cast<int>(bold.size()), 0);
    if (font_regular_ < 0 || font_bold_ < 0) {
        error_ = "failed to load the embedded fonts";
        return;
    }
    ok_ = true;
}

Window::~Window() {
    for (SDL_Cursor* c : cursors_) {
        if (c) SDL_DestroyCursor(c);
    }
    if (vg_) nvgDeleteGL3(vg_);
    if (gl_) SDL_GL_DestroyContext(gl_);
    if (window_) SDL_DestroyWindow(window_);
    SDL_Quit();
}

void Window::set_root(Widget& root) {
    root_ = &root;
    router_ = std::make_unique<EventRouter>(root);
    root.set_host(this);
    dirty_ = true;
}

void Window::set_focus(Widget* w) {
    if (router_) router_->set_focus(w);
}

Widget* Window::focus() const {
    return router_ ? router_->focus() : nullptr;
}

void Window::forget(Widget& subtree) {
    if (router_) router_->forget(subtree);
}

std::string Window::clipboard_text() {
    char* text = SDL_GetClipboardText();  // SDL allocates; we must free it
    std::string result = text ? text : "";
    SDL_free(text);
    return result;
}

void Window::set_clipboard_text(std::string_view text) {
    SDL_SetClipboardText(std::string(text).c_str());
}

bool mostly_visible(Rect window, const std::vector<Rect>& displays) {
    // The title bar must be reachable, or the window can't be moved back. SDL
    // positions are the client area's; the title bar sits ~30 px above it.
    const Rect title{window.x, window.y - 30, window.w, 50};
    for (const Rect& d : displays) {
        const float w = std::min(title.right(), d.right()) - std::max(title.x, d.x);
        const float h = std::min(title.bottom(), d.bottom()) - std::max(title.y, d.y);
        if (w >= std::min(100.0f, window.w) && h >= title.h) return true;
    }
    return false;
}

void Window::wake() {
    if (wake_event_ == 0) return;
    SDL_Event e{};
    e.type = wake_event_;
    SDL_PushEvent(&e);  // documented as safe from any thread
}

struct FolderDialog {
    // May run on a dialog thread (SDL docs), so it only stores the result.
    static void SDLCALL done(void* userdata, const char* const* files, int) {
        auto* w = static_cast<Window*>(userdata);
        {
            std::lock_guard lock(w->folder_mutex_);
            w->folder_done_ = true;
            if (files && files[0]) {
                // SDL paths are UTF-8; char8_t makes std::filesystem read them as such.
                w->folder_result_ = std::filesystem::path(reinterpret_cast<const char8_t*>(files[0]));
            }
        }
        w->wake();
    }
};

void Window::pick_folder(std::function<void(std::filesystem::path)> on_picked) {
    if (folder_dialog_open_ || !window_) return;  // one dialog at a time
    folder_dialog_open_ = true;
    folder_callback_ = std::move(on_picked);
    SDL_ShowOpenFolderDialog(&FolderDialog::done, this, window_, nullptr, false);
}

void Window::deliver_folder() {
    std::optional<std::filesystem::path> result;
    {
        std::lock_guard lock(folder_mutex_);
        if (!folder_done_) return;
        folder_done_ = false;
        result = std::move(folder_result_);
        folder_result_.reset();
    }
    folder_dialog_open_ = false;
    auto callback = std::move(folder_callback_);
    if (result && callback) callback(std::move(*result));
    dirty_ = true;
}

WindowPlacement fit_to_display(WindowPlacement p, Rect display) {
    p.w = std::min(p.w, static_cast<int>(display.w));
    p.h = std::min(p.h, static_cast<int>(display.h) - 40);  // title bar
    return p;
}

void Window::remember_normal_placement() {
    const SDL_WindowFlags f = SDL_GetWindowFlags(window_);
    if (f & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED | SDL_WINDOW_FULLSCREEN | SDL_WINDOW_HIDDEN)) return;
    SDL_GetWindowPosition(window_, &normal_.x, &normal_.y);
    SDL_GetWindowSize(window_, &normal_.w, &normal_.h);
}

WindowPlacement Window::placement() const {
    WindowPlacement p = normal_;
    p.maximized = window_ && (SDL_GetWindowFlags(window_) & SDL_WINDOW_MAXIMIZED);
    return p;
}

void Window::set_cursor(Cursor c) {
    static constexpr SDL_SystemCursor kSystem[] = {SDL_SYSTEM_CURSOR_DEFAULT, SDL_SYSTEM_CURSOR_EW_RESIZE,
                                                   SDL_SYSTEM_CURSOR_NS_RESIZE};
    const auto i = static_cast<std::size_t>(c);
    if (!cursors_[i]) cursors_[i] = SDL_CreateSystemCursor(kSystem[i]);
    if (cursors_[i]) SDL_SetCursor(cursors_[i]);
}

void Window::set_text_input(bool on) {
    if (on) SDL_StartTextInput(window_);
    else SDL_StopTextInput(window_);
}

float Window::ui_scale() const {
    // Display scale = how many framebuffer pixels one logical pixel takes
    // (1.5 at 150 % Windows scaling).
    const float s = SDL_GetWindowDisplayScale(window_);
    return s > 0 ? s : 1.0f;
}

float Window::to_logical() const {
    // SDL reports mouse positions in window coordinates. Pixel density turns
    // those into framebuffer pixels; dividing by the display scale gives
    // logical pixels, the unit every widget uses.
    return SDL_GetWindowPixelDensity(window_) / ui_scale();
}

void Window::handle(const SDL_Event& e, bool& running) {
    const float k = to_logical();
    switch (e.type) {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        running = false;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        MouseEvent m;
        if (!router_ || !map_button(e.button.button, m.button)) break;
        m.pos = {e.button.x * k, e.button.y * k};
        m.clicks = e.button.clicks;
        m.mods = map_mods(SDL_GetModState());
        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) router_->mouse_down(m);
        else router_->mouse_up(m);
        dirty_ = true;
        break;
    }
    case SDL_EVENT_MOUSE_MOTION:
        if (router_) {
            MouseEvent m;
            m.pos = {e.motion.x * k, e.motion.y * k};
            m.mods = map_mods(SDL_GetModState());
            const Widget* hovered_before = router_->hovered();
            router_->mouse_move(m);
            // Moving the mouse over the UI should not redraw everything: only
            // a change of the hovered widget or an active drag needs a frame.
            // Widgets whose look depends on the exact mouse position call
            // invalidate() themselves.
            if (router_->captured() || router_->hovered() != hovered_before) dirty_ = true;
        }
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        if (router_) {
            const float flip = e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0f : 1.0f;
            WheelEvent w{{e.wheel.mouse_x * k, e.wheel.mouse_y * k}, e.wheel.x * flip, e.wheel.y * flip,
                         map_mods(SDL_GetModState())};
            router_->wheel(w);
            dirty_ = true;
        }
        break;
    case SDL_EVENT_KEY_DOWN:
        if (router_) {
            router_->key({map_key(e.key.key), map_mods(e.key.mod), e.key.repeat});
            dirty_ = true;
        }
        break;
    case SDL_EVENT_TEXT_INPUT:
        if (router_) {
            router_->text({e.text.text});
            dirty_ = true;
        }
        break;
    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
        if (router_) router_->mouse_left_window();
        dirty_ = true;
        break;
    case SDL_EVENT_WINDOW_MOVED:
        remember_normal_placement();
        break;
    case SDL_EVENT_WINDOW_RESIZED:
        remember_normal_placement();
        [[fallthrough]];
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
    case SDL_EVENT_WINDOW_EXPOSED:
    case SDL_EVENT_WINDOW_RESTORED:
    case SDL_EVENT_WINDOW_MAXIMIZED:
        dirty_ = true;
        break;
    default:
        // A background thread woke us: something may have changed on screen.
        if (wake_event_ != 0 && e.type == wake_event_) dirty_ = true;
        break;
    }
}

void Window::draw_root(float width, float height, float pixel_ratio) {
    nvgBeginFrame(vg_, width, height, pixel_ratio);
    if (root_) {
        root_->layout({0, 0, width, height});
        Canvas canvas(vg_, font_regular_, font_bold_, pixel_ratio);
        root_->draw(canvas);
    }
    nvgEndFrame(vg_);
}

void Window::render_frame() {
    int pw = 0;
    int ph = 0;
    SDL_GetWindowSizeInPixels(window_, &pw, &ph);
    if (pw <= 0 || ph <= 0) return;
    const float s = ui_scale();
    glViewport(0, 0, pw, ph);
    clear_to_background();
    draw_root(static_cast<float>(pw) / s, static_cast<float>(ph) / s, s);
    SDL_GL_SwapWindow(window_);
}

void Window::run(const std::function<bool()>& before_frame) {
    if (!ok_) return;
    bool running = true;
    bool animating = false;
    dirty_ = true;
    SDL_Event e;
    while (running) {
        const bool minimized = (SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED) != 0;
        if (minimized) {
            // Nothing to draw. Idle: sleep until an event. While animating
            // (transport playing) before_frame still has work (device state),
            // but nobody sees the window, so 10 wake-ups a second are enough;
            // the audio itself runs on its own thread either way.
            if (animating ? SDL_WaitEventTimeout(&e, 100) : SDL_WaitEvent(&e)) handle(e, running);
        } else if (!dirty_ && !animating) {
            // Idle: sleep until the next event. 0 % CPU.
            if (SDL_WaitEvent(&e)) handle(e, running);
        }
        while (SDL_PollEvent(&e)) handle(e, running);
        deliver_folder();
        animating = before_frame ? before_frame() : false;
        if (!minimized && (dirty_ || animating)) {
            dirty_ = false;  // cleared before drawing: draw code may invalidate again
            render_frame();  // blocks on vsync while animating
        }
    }
}

bool Window::render_to_png(const std::string& path, int width, int height) {
    if (!ok_ || !root_) return false;
    NVGLUframebuffer* fb = nvgluCreateFramebuffer(vg_, width, height, 0);
    if (!fb) {
        error_ = "failed to create an offscreen framebuffer";
        return false;
    }
    nvgluBindFramebuffer(fb);
    glViewport(0, 0, width, height);
    clear_to_background();
    draw_root(static_cast<float>(width), static_cast<float>(height), 1.0f);

    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 4);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    nvgluBindFramebuffer(nullptr);
    nvgluDeleteFramebuffer(fb);

    // OpenGL's first row is the bottom of the image; PNG's is the top.
    std::vector<unsigned char> flipped(pixels.size());
    const std::size_t row = static_cast<std::size_t>(width) * 4;
    for (int y = 0; y < height; ++y) {
        std::memcpy(&flipped[static_cast<std::size_t>(y) * row], &pixels[static_cast<std::size_t>(height - 1 - y) * row], row);
    }
    // ABGR8888 in SDL's naming = bytes R, G, B, A in memory on little-endian.
    SDL_Surface* surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_ABGR8888, flipped.data(),
                                                 static_cast<int>(row));
    const bool saved = surface && SDL_SavePNG(surface, path.c_str());
    if (!saved) error_ = std::string("saving PNG: ") + SDL_GetError();
    SDL_DestroySurface(surface);
    return saved;
}

}  // namespace aloglas
