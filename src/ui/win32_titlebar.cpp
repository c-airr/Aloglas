// Dark title bar on Windows 10/11. Kept in its own file because <windows.h>
// defines macros (min, max, APIENTRY, ...) that clash with other headers.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>

namespace aloglas {

void use_dark_title_bar(void* hwnd) {
    if (!hwnd) return;
    // DWMWA_USE_IMMERSIVE_DARK_MODE = 20 (Windows 10 20H1 and later). Older
    // systems ignore it and keep the light title bar.
    BOOL on = TRUE;
    DwmSetWindowAttribute(static_cast<HWND>(hwnd), 20, &on, sizeof on);
}

}  // namespace aloglas
