// TEMPORARY entry point: opens the window with the palette preview.
//   aloglas                     window, keys 1/2/3 switch palettes
//   aloglas --screenshot a.png  render the current palette to a PNG and exit
//   aloglas --palettes <dir>    render all palette variants to <dir>/palette-N.png
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_main.h>

#include <string>

#include "app/demo/palette_preview.h"
#include "ui/theme.h"
#include "ui/window.h"

using namespace aloglas;

int main(int argc, char** argv) {
    std::string screenshot;
    std::string palettes_dir;
    for (int i = 1; i + 1 < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--screenshot") screenshot = argv[++i];
        else if (arg == "--palettes") palettes_dir = argv[++i];
    }
    const bool offscreen = !screenshot.empty() || !palettes_dir.empty();

    WindowOptions options;
    options.hidden = offscreen;
    Window window(options);
    if (!window.ok()) {
        SDL_Log("Aloglas: %s", window.error().c_str());
        return 1;
    }
    PalettePreview preview;
    window.set_root(preview);

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
    window.run([] { return false; });
    return 0;
}
