// Every color and size the UI uses, in one place. Widgets never hard-code a
// color: changing the look means changing a Theme, nothing else.
#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace aloglas {

struct Color {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
};

// rgb(0xF0A43A) -> Color{0xF0, 0xA4, 0x3A, 255}
constexpr Color rgb(uint32_t hex, uint8_t alpha = 255) {
    return {static_cast<uint8_t>(hex >> 16), static_cast<uint8_t>(hex >> 8), static_cast<uint8_t>(hex), alpha};
}

constexpr Color with_alpha(Color c, uint8_t alpha) {
    c.a = alpha;
    return c;
}

struct Theme {
    std::string name;

    // Surfaces
    Color background;
    Color panel;
    Color panel_header;
    Color panel_border;
    // Text
    Color text;
    Color text_dim;
    // One accent color: playhead, selection, focus border, active buttons.
    Color accent;
    Color accent_text;   // text drawn on top of the accent
    // Controls
    Color button;
    Color button_hover;
    Color button_on;
    Color field_bg;
    Color caret;
    Color selection;
    // Playlist / piano roll
    Color track_header;
    Color track_header_selected;
    Color lane;
    Color lane_alt;
    Color grid_bar;
    Color grid_beat;
    Color grid_sub;
    Color playhead;
    Color start_marker;
    Color loop_on;
    Color loop_off;
    // Meters and log levels
    Color meter_ok;
    Color meter_hot;
    Color warning;
    Color error;
    // Muted colors handed out to sounds (channel rack, drum rows, notes).
    std::array<Color, 12> sound_colors;

    // Metrics in logical pixels
    float font_size = 13;
    float font_size_small = 11;
    float transport_height = 40;
    float status_height = 24;
    float panel_header_height = 22;
    float header_width = 180;
    float loop_strip_height = 14;
    float bar_numbers_height = 18;
    float track_height = 48;
    float splitter = 4;
};

const Theme& theme();
void set_theme(const Theme& t);

// The palette candidates shown to the author (0..kThemeVariants-1).
inline constexpr int kThemeVariants = 3;
Theme theme_variant(int index);

}  // namespace aloglas
