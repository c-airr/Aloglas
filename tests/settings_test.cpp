#include <doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "app/settings.h"
#include "ui/window.h"

using namespace aloglas;
namespace fs = std::filesystem;

namespace {
// A fresh directory per test, removed afterwards.
struct TempDir {
    fs::path path;
    explicit TempDir(const char* name) : path(fs::temp_directory_path() / name) {
        fs::remove_all(path);
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

void write_file(const fs::path& p, const std::string& text) {
    std::ofstream(p, std::ios::binary) << text;
}
}  // namespace

TEST_CASE("settings round trip") {
    TempDir dir("aloglas_settings_round_trip");
    Settings s;
    s.window = WindowPlacement{120, 80, 1400, 900, false};
    s.panels.browser_width = 300;
    s.panels.rack_share = 0.5f;
    std::string error;
    REQUIRE(save_settings(dir.path / "sub" / "settings.json", s, &error));
    CHECK(error.empty());

    const SettingsLoad l = load_settings(dir.path / "sub" / "settings.json");
    CHECK(l.warning.empty());
    REQUIRE(l.settings.window.has_value());
    CHECK(l.settings.window->x == 120);
    CHECK(l.settings.window->y == 80);
    CHECK(l.settings.window->w == 1400);
    CHECK(l.settings.window->h == 900);
    CHECK_FALSE(l.settings.window->maximized);
    CHECK(l.settings.panels == s.panels);
}

TEST_CASE("load_settings: missing, corrupt, partial") {
    TempDir dir("aloglas_settings_bad");
    const fs::path p = dir.path / "settings.json";

    SettingsLoad l = load_settings(p);  // missing
    CHECK(l.warning.empty());
    CHECK_FALSE(l.settings.window.has_value());
    CHECK(l.settings.panels == PanelSizes{});

    write_file(p, "{not json");
    l = load_settings(p);
    CHECK_FALSE(l.warning.empty());
    CHECK(l.settings.panels == PanelSizes{});

    write_file(p, "[1, 2]");  // valid JSON, wrong shape
    l = load_settings(p);
    CHECK_FALSE(l.warning.empty());

    write_file(p, R"({"panels": {"browser_width": 300}})");
    l = load_settings(p);
    CHECK(l.warning.empty());
    CHECK(l.settings.panels.browser_width == 300);
    CHECK(l.settings.panels.right_width == 280);
    CHECK_FALSE(l.settings.window.has_value());

    write_file(p, R"({"panels": {"browser_width": "wide"}, "window": {"w": 1e30, "h": -5, "x": 1e30}})");
    l = load_settings(p);
    CHECK(l.settings.panels.browser_width == 220);
    REQUIRE(l.settings.window.has_value());  // absurd numbers are clamped, never undefined behaviour
    CHECK(l.settings.window->w <= 32768);
    CHECK(l.settings.window->h >= 100);
    CHECK(l.settings.window->x <= 32768);
}

TEST_CASE("save replaces an existing file") {
    TempDir dir("aloglas_settings_replace");
    const fs::path p = dir.path / "settings.json";
    write_file(p, "old content that is not json");
    Settings s;
    s.panels.right_width = 333;
    REQUIRE(save_settings(p, s, nullptr));
    CHECK(load_settings(p).settings.panels.right_width == 333);
    CHECK_FALSE(fs::exists(dir.path / "settings.json.tmp"));
}

TEST_CASE("a saved window position must still be on a display") {
    const std::vector<Rect> displays{{0, 0, 1920, 1080}, {1920, 0, 1280, 1024}};
    CHECK(mostly_visible({100, 100, 1280, 760}, displays));
    CHECK(mostly_visible({2000, 50, 1000, 700}, displays));     // second monitor
    CHECK(mostly_visible({1800, 100, 1280, 760}, displays));    // across both
    CHECK_FALSE(mostly_visible({10000, 100, 1280, 760}, displays));  // monitor unplugged
    CHECK_FALSE(mostly_visible({100, -2000, 1280, 760}, displays));
    CHECK_FALSE(mostly_visible({1900, 1060, 1280, 760}, displays));  // only a corner peeks out
    CHECK_FALSE(mostly_visible({100, 100, 1280, 760}, {}));
    CHECK_FALSE(mostly_visible({100, -700, 1280, 760}, displays));  // bottom visible, title bar above the screen
}

TEST_CASE("a window bigger than the display is shrunk to fit") {
    const Rect display{0, 0, 1920, 1040};
    const WindowPlacement big = fit_to_display({0, 0, 2000, 1200, false}, display);
    CHECK(big.w <= 1920);
    CHECK(big.h <= 1040 - 40);  // room for the title bar
    const WindowPlacement small = fit_to_display({5, 6, 1100, 700, true}, display);
    CHECK(small.w == 1100);
    CHECK(small.h == 700);
    CHECK(small.maximized);
}

TEST_CASE("settings keep non-ASCII browser folders") {
    TempDir dir("aloglas_settings_browser");
    const fs::path p = dir.path / "settings.json";
    Settings s;
    s.browser_folders = {fs::path(u8"C:/Sample żółć"), fs::path("D:/Splice")};
    s.preview = false;
    REQUIRE(save_settings(p, s, nullptr));
    const SettingsLoad l = load_settings(p);
    CHECK(l.settings.browser_folders == s.browser_folders);
    CHECK_FALSE(l.settings.preview);

    write_file(p, R"({"browser": {"folders": [1, "x", null]}})");
    const SettingsLoad partial = load_settings(p);
    REQUIRE(partial.settings.browser_folders.size() == 1);
    CHECK(partial.settings.browser_folders[0] == fs::path("x"));
    CHECK(partial.settings.preview);
}
