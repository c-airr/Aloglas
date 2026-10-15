#include "app/settings.h"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>

#include <algorithm>
#include <fstream>
#include <json.hpp>
#include <sstream>
#include <system_error>

namespace aloglas {

namespace {

using json = nlohmann::json;

// The file is user-editable, so every number is range-checked before it is
// used; converting an out-of-range double to int would be undefined behaviour.
double number_or(const json& obj, const char* key, double fallback, double lo, double hi) {
    const auto it = obj.find(key);
    if (it == obj.end() || !it->is_number()) return fallback;
    return std::clamp(it->get<double>(), lo, hi);
}

std::string utf8(const std::filesystem::path& p) {
    const std::u8string s = p.u8string();
    return {s.begin(), s.end()};
}

bool bool_or(const json& obj, const char* key, bool fallback) {
    const auto it = obj.find(key);
    return it != obj.end() && it->is_boolean() ? it->get<bool>() : fallback;
}

}  // namespace

SettingsLoad load_settings(const std::filesystem::path& path) {
    SettingsLoad result;
    std::ifstream in(path, std::ios::binary);
    if (!in) return result;  // first run: no file yet
    std::stringstream text;
    text << in.rdbuf();

    // allow_exceptions = false: invalid JSON gives a "discarded" value instead of throwing.
    const json root = json::parse(text.str(), nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        result.warning = "Settings unreadable, using defaults: " + utf8(path.filename()) +
                         (root.is_discarded() ? " is not valid JSON" : " has the wrong shape");
        return result;
    }

    if (const auto w = root.find("window"); w != root.end() && w->is_object()) {
        WindowPlacement p;
        p.x = static_cast<int>(number_or(*w, "x", p.x, -32768, 32768));
        p.y = static_cast<int>(number_or(*w, "y", p.y, -32768, 32768));
        p.w = static_cast<int>(number_or(*w, "w", p.w, 100, 32768));
        p.h = static_cast<int>(number_or(*w, "h", p.h, 100, 32768));
        p.maximized = bool_or(*w, "maximized", p.maximized);
        result.settings.window = p;
    }
    if (const auto s = root.find("panels"); s != root.end() && s->is_object()) {
        PanelSizes& p = result.settings.panels;
        p.browser_width = static_cast<float>(number_or(*s, "browser_width", p.browser_width, 0, 32768));
        p.right_width = static_cast<float>(number_or(*s, "right_width", p.right_width, 0, 32768));
        p.rack_share = static_cast<float>(number_or(*s, "rack_share", p.rack_share, 0, 1));
        p.piano_share = static_cast<float>(number_or(*s, "piano_share", p.piano_share, 0, 1));
    }
    if (const auto b = root.find("browser"); b != root.end() && b->is_object()) {
        if (const auto f = b->find("folders"); f != b->end() && f->is_array()) {
            for (const json& item : *f) {
                if (!item.is_string()) continue;
                const std::string s = item.get<std::string>();
                result.settings.browser_folders.emplace_back(std::u8string(s.begin(), s.end()));
            }
        }
        result.settings.preview = bool_or(*b, "preview", result.settings.preview);
    }
    return result;
}

bool save_settings(const std::filesystem::path& path, const Settings& settings, std::string* error) {
    json root;
    if (settings.window) {
        const WindowPlacement& w = *settings.window;
        root["window"] = {{"x", w.x}, {"y", w.y}, {"w", w.w}, {"h", w.h}, {"maximized", w.maximized}};
    }
    const PanelSizes& p = settings.panels;
    root["panels"] = {{"browser_width", p.browser_width},
                      {"right_width", p.right_width},
                      {"rack_share", p.rack_share},
                      {"piano_share", p.piano_share}};

    json folders = json::array();
    for (const std::filesystem::path& f : settings.browser_folders) folders.push_back(utf8(f));
    root["browser"] = {{"folders", folders}, {"preview", settings.preview}};

    auto fail = [&](const std::string& message) {
        if (error) *error = message;
        return false;
    };
    std::error_code ec;
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) return fail("cannot create " + utf8(path.parent_path()) + ": " + ec.message());

    std::filesystem::path tmp = path;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out << root.dump(2) << '\n';
        if (!out) return fail("cannot write " + utf8(tmp));
    }
    std::filesystem::rename(tmp, path, ec);  // replaces an existing file
    if (ec) {
        std::filesystem::remove(tmp, ec);
        return fail("cannot replace " + utf8(path));
    }
    return true;
}

std::filesystem::path settings_path() {
    // SDL returns UTF-8 (user names can have Polish letters); char8_t makes
    // std::filesystem read it as UTF-8 instead of the ANSI code page.
    char* dir = SDL_GetPrefPath(nullptr, "Aloglas");
    if (!dir) return "settings.json";
    std::filesystem::path p(reinterpret_cast<const char8_t*>(dir));
    SDL_free(dir);
    return p / "settings.json";
}

}  // namespace aloglas
