#include "ui/theme.h"

namespace aloglas {

namespace {

// Muted sound colors shared by all variants: desaturated enough not to fight
// with the accent, distinct enough to tell 12 sounds apart.
constexpr std::array<Color, 12> kSoundColors = {
    rgb(0xC9774B), rgb(0xC9A54B), rgb(0x8DAA4E), rgb(0x4FA37A), rgb(0x4B9DAF), rgb(0x5C7FC4),
    rgb(0x8A6CC4), rgb(0xB363A6), rgb(0xC45E6E), rgb(0x9C8B6E), rgb(0x6E9C9A), rgb(0x7C8A99),
};

// 1. Graphite: neutral grays with an amber accent.
Theme graphite() {
    Theme t;
    t.name = "Graphite";
    t.background = rgb(0x1C1C1C);
    t.panel = rgb(0x262626);
    t.panel_header = rgb(0x2E2E2E);
    t.panel_border = rgb(0x111111);
    t.text = rgb(0xD9D9D9);
    t.text_dim = rgb(0x8C8C8C);
    t.accent = rgb(0xF0A43A);
    t.accent_text = rgb(0x1A1A1A);
    t.button = rgb(0x353535);
    t.button_hover = rgb(0x404040);
    t.button_on = t.accent;
    t.field_bg = rgb(0x1A1A1A);
    t.caret = rgb(0xF0F0F0);
    t.selection = with_alpha(t.accent, 90);
    t.track_header = rgb(0x2C2C2C);
    t.track_header_selected = rgb(0x3A3A3A);
    t.lane = rgb(0x222222);
    t.lane_alt = rgb(0x252525);
    t.grid_bar = rgb(0x3D3D3D);
    t.grid_beat = rgb(0x2F2F2F);
    t.grid_sub = rgb(0x292929);
    t.playhead = t.accent;
    t.start_marker = rgb(0xC8C8C8);
    t.loop_on = t.accent;
    t.loop_off = rgb(0x5A5A5A);
    t.meter_ok = rgb(0x6CC070);
    t.meter_hot = rgb(0xE5484D);
    t.warning = rgb(0xE0C04A);
    t.error = rgb(0xE5484D);
    t.sound_colors = kSoundColors;
    return t;
}

// 2. Slate: cool blue-grays with a teal accent.
Theme slate() {
    Theme t = graphite();
    t.name = "Slate";
    t.background = rgb(0x1A1D21);
    t.panel = rgb(0x23272D);
    t.panel_header = rgb(0x2A2F36);
    t.panel_border = rgb(0x101316);
    t.text = rgb(0xD6DDE4);
    t.text_dim = rgb(0x84909C);
    t.accent = rgb(0x45C4B0);
    t.accent_text = rgb(0x0F1A18);
    t.button = rgb(0x313740);
    t.button_hover = rgb(0x3B424C);
    t.button_on = t.accent;
    t.field_bg = rgb(0x16191D);
    t.caret = rgb(0xF0F4F8);
    t.selection = with_alpha(t.accent, 90);
    t.track_header = rgb(0x2A2F36);
    t.track_header_selected = rgb(0x363D46);
    t.lane = rgb(0x1F2328);
    t.lane_alt = rgb(0x22262C);
    t.grid_bar = rgb(0x3A414A);
    t.grid_beat = rgb(0x2D3239);
    t.grid_sub = rgb(0x272B31);
    t.playhead = t.accent;
    t.start_marker = rgb(0xC0C8D0);
    t.loop_on = t.accent;
    t.loop_off = rgb(0x55606B);
    return t;
}

// 3. Ink: warm near-black with a coral accent.
Theme ink() {
    Theme t = graphite();
    t.name = "Ink";
    t.background = rgb(0x191716);
    t.panel = rgb(0x221F1D);
    t.panel_header = rgb(0x2A2725);
    t.panel_border = rgb(0x0E0D0C);
    t.text = rgb(0xE3DDD5);
    t.text_dim = rgb(0x8F877D);
    t.accent = rgb(0xE8694A);
    t.accent_text = rgb(0x1A0F0B);
    t.button = rgb(0x33302D);
    t.button_hover = rgb(0x3D3A36);
    t.button_on = t.accent;
    t.field_bg = rgb(0x151312);
    t.caret = rgb(0xF5F0EA);
    t.selection = with_alpha(t.accent, 90);
    t.track_header = rgb(0x2A2725);
    t.track_header_selected = rgb(0x38342F);
    t.lane = rgb(0x1E1C1A);
    t.lane_alt = rgb(0x211F1D);
    t.grid_bar = rgb(0x3D3935);
    t.grid_beat = rgb(0x2E2B28);
    t.grid_sub = rgb(0x282624);
    t.playhead = t.accent;
    t.start_marker = rgb(0xCFC6BB);
    t.loop_on = t.accent;
    t.loop_off = rgb(0x5E5852);
    return t;
}

Theme& current() {
    static Theme t = graphite();
    return t;
}

}  // namespace

const Theme& theme() {
    return current();
}

void set_theme(const Theme& t) {
    current() = t;
}

Theme theme_variant(int index) {
    switch (index) {
    case 1: return slate();
    case 2: return ink();
    default: return graphite();
    }
}

}  // namespace aloglas
