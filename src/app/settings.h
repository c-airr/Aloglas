// Per-user settings (not part of a project): window placement and panel
// sizes, in %APPDATA%\Aloglas\settings.json. Browser folders join later.
#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "app/main_layout.h"
#include "ui/window.h"

namespace aloglas {

struct Settings {
    std::optional<WindowPlacement> window;  // empty on the first run
    PanelSizes panels;
};

struct SettingsLoad {
    Settings settings;
    std::string warning;  // non-empty when the file was unreadable
};

// Missing file: defaults, no warning. Invalid JSON or a wrong shape: defaults
// plus a warning. A missing or mistyped field keeps its default.
SettingsLoad load_settings(const std::filesystem::path& path);

// Writes <path>.tmp and renames it over <path>, so a crash mid-write never
// leaves a half-written file. Creates the directory.
bool save_settings(const std::filesystem::path& path, const Settings& settings, std::string* error);

std::filesystem::path settings_path();

}  // namespace aloglas
