// Aloglas entry point.
//   aloglas                      the app
//   aloglas --no-audio           run without opening an audio device
//   aloglas --screenshot a.png   render the window to a PNG and exit
//   aloglas --palettes <dir>     render every palette variant to <dir>/palette-N.png
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_main.h>

#include <memory>
#include <string>

#include "app/controller.h"
#include "app/document.h"
#include "app/frame_update.h"
#include "app/live_state.h"
#include "app/log.h"
#include "app/main_view.h"
#include "app/settings.h"
#include "audio/audio_device.h"
#include "engine/engine.h"
#include "ui/theme.h"
#include "ui/window.h"

using namespace aloglas;

int main(int argc, char** argv) {
    std::string screenshot;
    std::string palettes_dir;
    bool no_audio = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--no-audio") no_audio = true;
        else if (arg == "--screenshot" && i + 1 < argc) screenshot = argv[++i];
        else if (arg == "--palettes" && i + 1 < argc) palettes_dir = argv[++i];
    }
    const bool offscreen = !screenshot.empty() || !palettes_dir.empty();

    Engine engine;
    std::unique_ptr<AudioDevice> device;
    if (!no_audio && !offscreen) device = std::make_unique<AudioDevice>(engine);

    // Screenshots ignore the user's settings so they always look the same.
    SettingsLoad settings;
    if (!offscreen) settings = load_settings(settings_path());

    WindowOptions options;
    options.hidden = offscreen;
    options.placement = settings.settings.window;
    Window window(options);
    if (!window.ok()) {
        SDL_Log("Aloglas: %s", window.error().c_str());
        return 1;
    }

    Log log;
    log.on_change = [&] { window.invalidate(); };
    Document doc;
    Controller controller(doc, engine);
    controller.on_change = [&] { window.invalidate(); };
    // Play opens the device when it isn't running (first start failed, or
    // the device was lost), so plugging headphones back in and pressing Play
    // brings sound back.
    controller.ensure_audio = [&] {
        if (!device) return false;
        if (device->status() == AudioStatus::Running) return true;
        if (device->open()) {
            log.add(LogLevel::Info, "Audio: " + std::to_string(device->sample_rate()) + " Hz, " +
                                        std::to_string(device->buffer_frames()) + " samples");
            return true;
        }
        log.add(LogLevel::Error, "Audio error: " + device->error());
        return false;
    };

    LiveState live;
    MainView view(controller, live, log);
    view.sizes() = settings.settings.panels;
    window.set_root(view);
    if (!settings.warning.empty()) log.add(LogLevel::Warning, settings.warning);

    controller.sync_engine();
    engine.pump();
    if (device) controller.ensure_audio();
    frame_update(engine, device.get(), controller, log, live);

    if (!palettes_dir.empty()) {
        for (int i = 0; i < kThemeVariants; ++i) {
            set_theme(theme_variant(i));
            const std::string path = palettes_dir + "/palette-" + std::to_string(i + 1) + ".png";
            if (!window.render_to_png(path, 1280, 760)) {
                SDL_Log("Aloglas: %s", window.error().c_str());
                return 1;
            }
        }
        return 0;
    }
    if (!screenshot.empty()) {
        if (!window.render_to_png(screenshot, 1280, 760)) {
            SDL_Log("Aloglas: %s", window.error().c_str());
            return 1;
        }
        return 0;
    }

    window.run([&] {
        frame_update(engine, device.get(), controller, log, live);
        return needs_frames(live);
    });
    if (device) device->close();

    std::string error;
    if (!save_settings(settings_path(), {window.placement(), view.sizes()}, &error)) {
        SDL_Log("Aloglas: settings not saved: %s", error.c_str());  // the log panel is gone by now
    }
    return 0;
}
