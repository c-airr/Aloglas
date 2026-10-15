#include "app/demo/palette_preview.h"

#include <cmath>
#include <numbers>
#include <string>
#include <vector>

#include "ui/canvas.h"
#include "ui/theme.h"

// TEMPORARY mock. Dense Ableton-like style: controls in a group touch and
// share 1-px borders, square corners, small text, low rows.

namespace aloglas {

namespace {

using Align = Canvas::Align;
constexpr float kPi = std::numbers::pi_v<float>;

// --- small drawing helpers -------------------------------------------------

// A control box: fill + 1-px outline. Boxes placed with x += w - 1 share an edge.
void box(Canvas& c, const Theme& t, Rect r, std::string_view label, bool on = false, bool bold = false) {
    c.fill_rect(r, on ? t.button_on : t.button);
    c.stroke_rect(r, t.panel_border);
    if (!label.empty()) {
        c.text(r, label, on ? t.accent_text : t.text, t.font_size_small, Align::Center, bold || on);
    }
}

// A row of touching boxes; returns the x after the group.
float group(Canvas& c, const Theme& t, float x, float y, float h, const std::vector<std::pair<std::string, float>>& items,
            int active = -1) {
    for (std::size_t i = 0; i < items.size(); ++i) {
        box(c, t, {x, y, items[i].second, h}, items[i].first, static_cast<int>(i) == active);
        x += items[i].second - 1;
    }
    return x + 1;
}

// Dark value field with an optional dim label on the left: "Root  C5".
void field(Canvas& c, const Theme& t, Rect r, std::string_view label, std::string_view value, bool bold_value = true) {
    c.fill_rect(r, t.field_bg);
    c.stroke_rect(r, t.panel_border);
    if (label.empty()) {
        c.text(r, value, t.text, t.font_size, Align::Center, bold_value);
        return;
    }
    c.text({r.x + 6, r.y, r.w - 12, r.h}, label, t.text_dim, t.font_size_small);
    c.text({r.x + 6, r.y, r.w - 12, r.h}, value, t.text, t.font_size_small, Align::Right, bold_value);
}

void panel_header(Canvas& c, const Theme& t, Rect r, std::string_view title) {
    c.fill_rect(r, t.panel_header);
    c.hline(r.x, r.right(), r.bottom() - 1, t.panel_border);
    c.text({r.x + 8, r.y, r.w - 16, r.h}, title, t.text_dim, t.font_size_small, Align::Left, true);
}

void knob(Canvas& c, const Theme& t, Point center, float value) {
    const float r = 6;
    const float start = kPi * 0.75f;
    const float end = kPi * 2.25f;
    c.arc(center, r, start, end, t.button_hover, 1.5f);
    c.arc(center, r, start, start + (end - start) * value, t.text, 1.5f);
}

void tri_right(Canvas& c, Point p, Color col) {
    c.fill_triangle({p.x, p.y - 3.5f}, {p.x + 4.5f, p.y}, {p.x, p.y + 3.5f}, col);
}

void tri_down(Canvas& c, Point p, Color col) {
    c.fill_triangle({p.x - 0.5f, p.y - 2}, {p.x + 6.5f, p.y - 2}, {p.x + 3, p.y + 2.5f}, col);
}

// --- panels ------------------------------------------------------------------

void transport(Canvas& c, const Theme& t, Rect r) {
    c.fill_rect(r, t.panel);
    c.hline(r.x, r.right(), r.bottom() - 1, t.panel_border);
    const float h = t.control_height;
    const float y = r.y + (r.h - h) / 2;
    const float cy = y + h / 2;
    float x = r.x + 8;

    // menu
    box(c, t, {x, y, 24, h}, "");
    for (int i = -1; i <= 1; ++i) c.hline(x + 7, x + 17, cy + i * 3.5f, t.text);
    x += 24 + t.group_gap;

    // play / stop
    box(c, t, {x, y, 28, h}, "", true);
    c.fill_triangle({x + 11, cy - 4}, {x + 18, cy}, {x + 11, cy + 4}, t.accent_text);
    box(c, t, {x + 27, y, 28, h}, "");
    c.fill_rect({x + 27 + 10, cy - 3.5f, 7, 7}, t.text);
    x += 55 + t.group_gap;

    // tempo + signature
    field(c, t, {x, y, 52, h}, "", "140.00");
    c.text({x + 56, y, 24, h}, "BPM", t.text_dim, t.font_size_small);
    x += 80;
    field(c, t, {x, y, 44, h}, "", "4 / 4", false);
    x += 44 + t.group_gap;

    // modes and toggles
    x = group(c, t, x, y, h, {{"LOOP", 46}, {"SONG", 46}}, 0) + t.group_gap;
    x = group(c, t, x, y, h, {{"Metro", 46}}, 0);
    x = group(c, t, x - 1, y, h, {{"Brace", 46}}) ;
    x = group(c, t, x - 1, y, h, {{"Follow", 50}}, 0) + t.group_gap;

    // position
    c.fill_rect({x, y, 150, h}, t.field_bg);
    c.stroke_rect({x, y, 150, h}, t.panel_border);
    c.text({x + 8, y, 60, h}, "3.2.1", t.text, t.font_size, Align::Left, true);
    c.text({x + 64, y, 80, h}, "00:04.285", t.text_dim, t.font_size);

    // CPU + master meter
    const float rx = r.right() - 176;
    c.text({rx, y, 48, h}, "CPU 7%", t.text_dim, t.font_size_small);
    const float mx = r.right() - 110;
    c.fill_rect({mx, cy - 6, 100, 5}, t.field_bg);
    c.fill_rect({mx, cy + 1, 100, 5}, t.field_bg);
    c.fill_rect({mx, cy - 6, 68, 5}, t.meter_ok);
    c.fill_rect({mx, cy + 1, 63, 5}, t.meter_ok);
}

void browser(Canvas& c, const Theme& t, Rect r) {
    c.fill_rect(r, t.panel);
    Rect body = r;
    const Rect head = cut_top(body, t.panel_header_height);
    panel_header(c, t, head, "BROWSER");
    box(c, t, {head.right() - 58, head.y + 2, 54, head.h - 4}, "+ Folder");

    struct Row { int depth; bool folder; bool open; const char* name; bool selected; };
    const Row rows[] = {
        {0, true, true, "Splice", false},           {1, true, true, "Trap Drums", false},
        {2, false, false, "kick_808_hard.wav", true}, {2, false, false, "kick_punchy.wav", false},
        {2, false, false, "snare_crack.wav", false},  {2, false, false, "clap_tight.wav", false},
        {2, false, false, "hat_closed_01.wav", false}, {2, false, false, "hat_open_02.wav", false},
        {1, true, false, "808s", false},             {1, true, false, "Melodies", false},
        {0, true, false, "My Samples", false},
    };
    const float rh = t.row_height;
    float y = body.y + 2;
    for (const Row& row : rows) {
        const Rect line{body.x, y, body.w, rh};
        if (row.selected) c.fill_rect(line, t.selection);
        const float x = body.x + 8 + row.depth * 12.0f;
        if (row.folder) {
            if (row.open) tri_down(c, {x, y + rh / 2}, t.text_dim);
            else tri_right(c, {x + 1, y + rh / 2}, t.text_dim);
        }
        c.text({x + 12, y, body.right() - x - 16, rh}, row.name, row.selected || row.folder ? t.text : t.text_dim,
               t.font_size);
        y += rh;
    }
}

void playlist(Canvas& c, const Theme& t, Rect r) {
    c.fill_rect(r, t.lane);
    Rect body = r;
    panel_header(c, t, cut_top(body, t.panel_header_height), "PLAYLIST");
    const float header_w = t.header_width;
    Rect ruler = cut_top(body, t.loop_strip_height + t.bar_numbers_height);
    const Rect ruler_left = cut_left(ruler, header_w);
    c.fill_rect(ruler_left, t.panel);
    c.fill_rect(ruler, t.panel_header);
    c.vline(ruler_left.right(), ruler_left.y, body.bottom(), t.panel_border);

    const float x0 = ruler.x;
    const float bar_w = 112;  // 28 px per quarter note
    c.fill_rect({x0, ruler.y + 2, bar_w * 4, t.loop_strip_height - 4}, t.loop_on);
    for (int bar = 0; bar * bar_w < ruler.w; ++bar) {
        const float x = x0 + bar * bar_w;
        c.vline(x, ruler.y + t.loop_strip_height, ruler.bottom(), t.grid_bar);
        c.text({x + 3, ruler.y + t.loop_strip_height, 36, t.bar_numbers_height}, std::to_string(bar + 1), t.text_dim,
               t.font_size_small);
    }
    c.fill_triangle({x0, ruler.bottom() - 6}, {x0 + 5, ruler.bottom() - 6}, {x0, ruler.bottom() - 1}, t.start_marker);
    c.hline(r.x, r.right(), ruler.bottom() - 1, t.panel_border);

    const int tracks = 4;
    const float th = t.track_height;
    const char* names[] = {"1 Track", "2 Track", "3 Track", "4 Track"};
    for (int i = 0; i < tracks; ++i) {
        const float y = body.y + i * th;
        const Rect head{body.x, y, header_w, th};
        c.fill_rect(head, i == 1 ? t.track_header_selected : t.track_header);
        c.fill_rect({head.x, y, 3, th - 1}, t.sound_colors[static_cast<size_t>(i * 3)]);
        c.text({head.x + 10, y + 2, head.w - 40, 14}, names[i], t.text, t.font_size);
        box(c, t, {head.right() - 20, y + 3, 14, 13}, "M");
        c.fill_rect({x0, y, body.right() - x0, th}, i % 2 ? t.lane_alt : t.lane);
        c.hline(body.x, body.right(), y + th - 1, t.panel_border);
    }
    const float tracks_bottom = body.y + tracks * th;
    c.fill_rect({body.x, tracks_bottom, header_w, body.bottom() - tracks_bottom}, t.panel);

    c.push_clip({x0, body.y, body.right() - x0, tracks * th});
    for (int q = 0; q * 28.0f < body.w; ++q) {
        const float x = x0 + q * 28.0f;
        c.vline(x, body.y, tracks_bottom, q % 4 == 0 ? t.grid_bar : t.grid_beat);
        c.vline(x + 14, body.y, tracks_bottom, t.grid_sub);
    }
    auto clip = [&](int track, float bar_from, float bars) {
        const Rect cr{x0 + bar_from * bar_w + 1, body.y + track * th + 1, bars * bar_w - 1, th - 2};
        c.fill_rect(cr, t.button);
        c.fill_rect({cr.x, cr.y, cr.w, 12}, t.loop_off);
        c.stroke_rect(cr, t.panel_border);
        c.text({cr.x + 5, cr.y, cr.w - 10, 12}, "Loop", t.text, t.font_size_small, Align::Left, true);
        for (int rep = 0; rep < static_cast<int>(bars / 2); ++rep) {
            const float bx = cr.x + rep * 2 * bar_w;
            if (rep > 0) c.vline(bx, cr.y + 12, cr.bottom() - 1, t.panel_border);
            for (int n = 0; n < 16; ++n) {
                const int sound = n % 4;
                c.fill_rect({bx + 3 + n * 14.0f, cr.y + 14 + sound * 4.0f, 9, 3},
                            t.sound_colors[static_cast<size_t>(sound * 3)]);
            }
        }
    };
    clip(0, 0, 4);
    clip(0, 4, 4);
    clip(1, 2, 2);
    clip(2, 4, 4);
    c.pop_clip();
    c.vline(x0 + bar_w * 1.55f, ruler.y + t.loop_strip_height, tracks_bottom, t.playhead);
}

void channel_rack(Canvas& c, const Theme& t, Rect r) {
    c.fill_rect(r, t.panel);
    Rect body = r;
    panel_header(c, t, cut_top(body, t.panel_header_height), "CHANNEL RACK");
    struct Ch { const char* name; bool on; int color; float vol; float pan; };
    const Ch rows[] = {{"Kick", true, 0, 0.8f, 0.5f},     {"Snare", true, 3, 0.7f, 0.5f},
                       {"Hat", true, 6, 0.6f, 0.62f},     {"Open Hat", false, 9, 0.55f, 0.4f},
                       {"808", true, 5, 0.85f, 0.5f},     {"Perc", false, 8, 0.5f, 0.7f}};
    const float rh = 20;
    float y = body.y;
    int i = 0;
    for (const Ch& ch : rows) {
        const Rect row{body.x, y, body.w, rh};
        c.fill_rect(row, i == 2 ? t.track_header_selected : t.track_header);
        c.fill_circle({row.x + 11, y + rh / 2}, 3.5f, ch.on ? t.accent : t.button_hover);
        c.fill_rect({row.x + 20, y + 1, 3, rh - 2}, t.sound_colors[static_cast<size_t>(ch.color)]);
        c.text({row.x + 30, y, row.w - 100, rh}, ch.name, ch.on ? t.text : t.text_dim, t.font_size);
        knob(c, t, {row.right() - 44, y + rh / 2}, ch.vol);
        knob(c, t, {row.right() - 20, y + rh / 2}, ch.pan);
        c.hline(row.x, row.right(), y + rh - 1, t.panel_border);
        y += rh;
        ++i;
    }
}

void log_panel(Canvas& c, const Theme& t, Rect r) {
    c.fill_rect(r, t.panel);
    Rect body = r;
    const Rect head = cut_top(body, t.panel_header_height);
    panel_header(c, t, head, "LOG");
    box(c, t, {head.right() - 44, head.y + 2, 40, head.h - 4}, "Clear");
    struct Line { const char* time; const char* msg; int level; };
    const Line lines[] = {{"12:04:31", "Audio: 48000 Hz, 480 samples", 0},
                          {"12:04:40", "Loaded kick_808_hard.wav", 0},
                          {"12:05:02", "Missing sample: vox_chop.wav", 1},
                          {"12:05:10", "Saved untitled.aloglas", 0},
                          {"12:05:44", "Export failed: file is in use", 2}};
    float y = body.y + 2;
    for (const Line& l : lines) {
        c.text({body.x + 8, y, 52, t.row_height}, l.time, t.text_dim, t.font_size_small);
        const Color col = l.level == 2 ? t.error : (l.level == 1 ? t.warning : t.text);
        c.text({body.x + 60, y, body.w - 66, t.row_height}, l.msg, col, t.font_size_small);
        y += t.row_height;
    }
}

void piano_roll(Canvas& c, const Theme& t, Rect r) {
    c.fill_rect(r, t.panel);
    Rect body = r;
    const Rect head = cut_top(body, t.panel_header_height + 4);
    c.fill_rect(head, t.panel_header);
    c.hline(head.x, head.right(), head.bottom() - 1, t.panel_border);
    const float h = t.control_height;
    const float y = head.y + (head.h - h) / 2;
    float x = head.x + 6;
    x = group(c, t, x, y, h, {{"Keys", 42}, {"Drums", 46}}, 0) + t.group_gap;
    // sound selector
    c.fill_rect({x, y, 96, h}, t.field_bg);
    c.stroke_rect({x, y, 96, h}, t.panel_border);
    c.fill_rect({x + 1, y + 1, 3, h - 2}, t.sound_colors[5]);
    c.text({x + 10, y, 60, h}, "808", t.text, t.font_size_small, Align::Left, true);
    tri_down(c, {x + 84, y + h / 2}, t.text_dim);
    x += 96 + t.group_gap;
    field(c, t, {x, y, 66, h}, "Root", "C5");
    field(c, t, {x + 65, y, 56, h}, "Fine", "0");
    x += 121 + t.group_gap;
    x = group(c, t, x, y, h, {{"Cut itself", 64}}, 0);
    x = group(c, t, x - 1, y, h, {{"Gate", 40}}, 0) + t.group_gap;
    field(c, t, {x, y, 86, h}, "Glide", "80 ms");
    x += 86 + t.group_gap;
    field(c, t, {x, y, 82, h}, "Length", "Auto");
    x += 82 + t.group_gap;
    box(c, t, {x, y, 46, h}, "Roll");
    // drag handle to the playlist
    const float gx = head.right() - 58;
    box(c, t, {gx, y, 52, h}, "");
    for (int i = 0; i < 3; ++i) {
        c.fill_circle({gx + 8, y + 5 + i * 5.0f}, 1.1f, t.text_dim);
        c.fill_circle({gx + 12, y + 5 + i * 5.0f}, 1.1f, t.text_dim);
    }
    c.text({gx + 18, y, 32, h}, "Loop", t.text, t.font_size_small, Align::Left, true);

    const float vel_h = 40;
    Rect vel = cut_bottom(body, vel_h);
    const Rect keys = cut_left(body, 48);
    const float row_h = 10;
    const int rows = static_cast<int>(body.h / row_h) + 1;
    const int top_pitch = 66;
    for (int i = 0; i < rows; ++i) {
        const int pitch = top_pitch - i;
        const int pc = ((pitch % 12) + 12) % 12;
        const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        const float ry = body.y + i * row_h;
        c.fill_rect({keys.x, ry, keys.w, row_h}, black ? rgb(0x171717) : rgb(0xB8B8B8));
        c.hline(keys.x, keys.right(), ry + row_h - 1, rgb(0x4A4A4A));
        c.fill_rect({body.x, ry, body.w, row_h}, black ? t.lane : t.lane_alt);
        if (pc == 0) {
            c.text({keys.x + 22, ry, 24, row_h}, "C" + std::to_string(pitch / 12), rgb(0x1A1A1A), 8);
            c.hline(body.x, body.right(), ry + row_h - 1, t.grid_bar);
        }
    }
    c.vline(keys.right(), body.y, body.bottom(), t.panel_border);
    c.push_clip(body);
    const float q = 40;
    for (int i = 0; i * q / 4 < body.w; ++i) {
        const float gx2 = body.x + i * q / 4;
        c.vline(gx2, body.y, body.bottom(), i % 16 == 0 ? t.grid_bar : (i % 4 == 0 ? t.grid_beat : t.grid_sub));
    }
    auto note_y = [&](int pitch) { return body.y + (top_pitch - pitch) * row_h; };
    for (int i = 0; i < 16; ++i) {
        c.fill_rect({body.x + i * q / 2 + 1, note_y(60) + 1, q / 2 - 2, row_h - 2}, t.button_hover);
    }
    struct N { float start; float len; int pitch; bool slide; };
    const N notes[] = {{0, 3, 55, false}, {3, 1, 58, true}, {4, 2, 53, false}, {6, 2, 55, false},
                       {8, 3, 55, false}, {11, 1, 50, true}, {12, 4, 53, false}};
    const Color sc = t.sound_colors[5];
    for (const N& n : notes) {
        const Rect nr{body.x + n.start * q + 1, note_y(n.pitch) + 1, n.len * q - 1, row_h - 1};
        c.fill_rect(nr, n.start == 4 ? t.accent : sc);
        c.stroke_rect(nr, t.panel_border);
        if (n.slide) c.fill_triangle({nr.x + 2, nr.bottom() - 1}, {nr.x + 7, nr.y + 1}, {nr.x + 7, nr.bottom() - 1}, t.text);
    }
    c.vline(body.x + q * 5.4f, body.y, body.bottom(), t.playhead);
    c.pop_clip();

    c.fill_rect(vel, t.panel_header);
    c.hline(vel.x, vel.right(), vel.y, t.panel_border);
    c.text({vel.x + 4, vel.y + 2, 46, 12}, "Velocity", t.text_dim, 9);
    for (const N& n : notes) {
        const float v = 0.55f + 0.06f * static_cast<float>(static_cast<int>(n.start) % 5);
        const float bx = body.x + n.start * q + 1;
        const float bh = (vel.h - 8) * v;
        c.vline(bx, vel.bottom() - 3 - bh, vel.bottom() - 3, sc);
        c.fill_rect({bx - 2, vel.bottom() - 3 - bh - 2, 5, 3}, sc);
    }
    c.stroke_rect(r, t.accent);  // focused panel
}

}  // namespace

void PalettePreview::draw(Canvas& c) {
    const Theme& t = theme();
    c.fill_rect(rect, t.panel_border);
    Rect area = rect;
    transport(c, t, cut_top(area, t.transport_height));
    const Rect status = cut_bottom(area, t.status_height);
    // Panels touch; the 1-px dark background between them is the divider.
    const Rect left = cut_left(area, 220);
    browser(c, t, {left.x, left.y, left.w - 1, left.h});

    Rect top = area;
    const Rect bottom = cut_bottom(top, std::round(area.h * 0.42f));
    const Rect right = cut_right(top, 280);
    playlist(c, t, {top.x, top.y, top.w - 1, top.h - 1});
    Rect rc = right;
    const Rect log_r = cut_bottom(rc, std::round(right.h * 0.4f));
    channel_rack(c, t, {rc.x, rc.y, rc.w, rc.h - 1});
    log_panel(c, t, {log_r.x, log_r.y, log_r.w, log_r.h - 1});
    piano_roll(c, t, bottom);

    c.fill_rect(status, t.panel_header);
    c.hline(status.x, status.right(), status.y, t.panel_border);
    c.text({status.x + 8, status.y, 500, status.h}, "Audio: 48000 Hz, 480 samples  \xC2\xB7  untitled.aloglas *",
           t.text_dim, t.font_size_small);
    c.text({status.right() - 410, status.y, 402, status.h}, "Palette preview: " + t.name + "   (keys 1 / 2 / 3)",
           t.text_dim, t.font_size_small, Align::Right);
}

bool PalettePreview::on_key(const KeyEvent& e) {
    int variant = -1;
    if (e.key == Key::D1) variant = 0;
    if (e.key == Key::D2) variant = 1;
    if (e.key == Key::D3) variant = 2;
    if (variant < 0) return false;
    set_theme(theme_variant(variant));
    invalidate();
    return true;
}

}  // namespace aloglas
