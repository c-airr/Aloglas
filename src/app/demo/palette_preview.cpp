#include "app/demo/palette_preview.h"

#include <cmath>
#include <numbers>
#include <string>

#include "ui/canvas.h"
#include "ui/theme.h"

namespace aloglas {

namespace {

using Align = Canvas::Align;
constexpr float kPi = std::numbers::pi_v<float>;

void panel_header(Canvas& c, const Theme& t, Rect r, const char* title) {
    c.fill_rect(r, t.panel_header);
    c.hline(r.x, r.right(), r.bottom() - 1, t.panel_border);
    c.text({r.x + 8, r.y, r.w - 16, r.h}, title, t.text, t.font_size_small, Align::Left, true);
}

void button(Canvas& c, const Theme& t, Rect r, const char* label, bool on) {
    c.fill_rect(r, on ? t.button_on : t.button, 2);
    c.text(r, label, on ? t.accent_text : t.text, t.font_size_small, Align::Center, on);
}

void field(Canvas& c, const Theme& t, Rect r, const std::string& s, bool bold) {
    c.fill_rect(r, t.field_bg, 2);
    c.text(r, s, t.text, t.font_size, Align::Center, bold);
}

void knob(Canvas& c, const Theme& t, Point center, float value) {
    const float r = 7;
    const float start = kPi * 0.75f;
    const float end = kPi * 2.25f;
    c.arc(center, r, start, end, t.button_hover, 2);
    c.arc(center, r, start, start + (end - start) * value, t.text_dim, 2);
}

void tri_right(Canvas& c, Point p, Color col) {
    c.fill_triangle({p.x, p.y - 4}, {p.x + 5, p.y}, {p.x, p.y + 4}, col);
}

void tri_down(Canvas& c, Point p, Color col) {
    c.fill_triangle({p.x - 1, p.y - 2}, {p.x + 7, p.y - 2}, {p.x + 3, p.y + 3}, col);
}

void transport(Canvas& c, const Theme& t, Rect r) {
    c.fill_rect(r, t.panel);
    c.hline(r.x, r.right(), r.bottom() - 1, t.panel_border);
    float x = r.x + 10;
    const float cy = r.y + r.h / 2;
    // menu
    for (int i = -1; i <= 1; ++i) c.hline(x, x + 14, cy + i * 4.0f, t.text_dim);
    x += 30;
    // play (playing) and stop
    c.fill_rect({x, cy - 12, 30, 24}, t.button_on, 2);
    c.fill_triangle({x + 11, cy - 6}, {x + 21, cy}, {x + 11, cy + 6}, t.accent_text);
    x += 34;
    c.fill_rect({x, cy - 12, 30, 24}, t.button, 2);
    c.fill_rect({x + 10, cy - 5, 10, 10}, t.text);
    x += 44;
    field(c, t, {x, cy - 12, 64, 24}, "140.00", true);
    c.text({x + 68, cy - 12, 30, 24}, "BPM", t.text_dim, t.font_size_small);
    x += 104;
    field(c, t, {x, cy - 12, 48, 24}, "4 / 4", false);
    x += 62;
    button(c, t, {x, cy - 11, 46, 22}, "LOOP", true);
    button(c, t, {x + 47, cy - 11, 46, 22}, "SONG", false);
    x += 108;
    button(c, t, {x, cy - 11, 50, 22}, "Metro", true);
    button(c, t, {x + 54, cy - 11, 50, 22}, "Brace", false);
    button(c, t, {x + 108, cy - 11, 54, 22}, "Follow", true);
    x += 176;
    field(c, t, {x, cy - 12, 168, 24}, "3.2.1    00:04.285", false);

    // CPU and master meter on the right
    const float rx = r.right() - 190;
    c.text({rx, cy - 12, 60, 24}, "CPU 7%", t.text_dim, t.font_size_small);
    c.fill_rect({rx + 52, cy - 3, 40, 6}, t.field_bg, 1);
    c.fill_rect({rx + 52, cy - 3, 4, 6}, t.text_dim, 1);
    const float mx = r.right() - 80;
    c.fill_rect({mx, cy - 7, 64, 5}, t.field_bg, 1);
    c.fill_rect({mx, cy + 2, 64, 5}, t.field_bg, 1);
    c.fill_rect({mx, cy - 7, 44, 5}, t.meter_ok, 1);
    c.fill_rect({mx, cy + 2, 40, 5}, t.meter_ok, 1);
}

void browser(Canvas& c, const Theme& t, Rect r) {
    c.fill_rect(r, t.panel);
    Rect body = r;
    const Rect head = cut_top(body, t.panel_header_height);
    panel_header(c, t, head, "Browser");
    button(c, t, {head.right() - 64, head.y + 3, 58, head.h - 6}, "+ Folder", false);

    struct Row { int depth; bool folder; bool open; const char* name; bool selected; };
    const Row rows[] = {
        {0, true, true, "Splice", false},       {1, true, true, "Trap Drums", false},
        {2, false, false, "kick_808_hard.wav", true}, {2, false, false, "kick_punchy.wav", false},
        {2, false, false, "snare_crack.wav", false},  {2, false, false, "clap_tight.wav", false},
        {2, false, false, "hat_closed_01.wav", false}, {2, false, false, "hat_open_02.wav", false},
        {1, true, false, "808s", false},        {1, true, false, "Melodies", false},
        {0, true, false, "My Samples", false},
    };
    float y = body.y + 4;
    for (const Row& row : rows) {
        const Rect line{body.x, y, body.w, 22};
        if (row.selected) c.fill_rect(line, t.selection);
        const float x = body.x + 10 + row.depth * 14.0f;
        if (row.folder) {
            if (row.open) tri_down(c, {x, y + 11}, t.text_dim);
            else tri_right(c, {x + 1, y + 11}, t.text_dim);
        }
        c.text({x + 14, y, body.right() - x - 18, 22}, row.name, row.folder ? t.text : t.text_dim, t.font_size,
               Align::Left, false);
        y += 22;
    }
}

void playlist(Canvas& c, const Theme& t, Rect r) {
    c.fill_rect(r, t.panel);
    Rect body = r;
    panel_header(c, t, cut_top(body, t.panel_header_height), "Playlist");
    const float header_w = 150;
    Rect ruler = cut_top(body, t.loop_strip_height + t.bar_numbers_height);
    const Rect ruler_left = cut_left(ruler, header_w);
    c.fill_rect(ruler_left, t.panel);
    c.fill_rect(ruler, t.panel_header);

    const float x0 = ruler.x;
    const float bar_w = 120;  // 30 px per quarter note
    // loop brace bars 1-5, start marker, bar numbers
    c.fill_rect({x0, ruler.y + 3, bar_w * 4, t.loop_strip_height - 6}, t.loop_on, 2);
    for (int bar = 0; bar * bar_w < ruler.w; ++bar) {
        const float x = x0 + bar * bar_w;
        c.vline(x, ruler.y + t.loop_strip_height, ruler.bottom(), t.grid_bar);
        c.text({x + 4, ruler.y + t.loop_strip_height, 40, t.bar_numbers_height}, std::to_string(bar + 1),
               t.text_dim, t.font_size_small);
    }
    c.fill_triangle({x0, ruler.bottom() - 7}, {x0 + 6, ruler.bottom() - 7}, {x0, ruler.bottom() - 1},
                    t.start_marker);
    c.hline(r.x, r.right(), ruler.bottom() - 1, t.panel_border);

    // tracks
    const char* names[] = {"1 Track", "2 Track", "3 Track", "4 Track"};
    for (int i = 0; i < 4; ++i) {
        const float y = body.y + i * t.track_height;
        const Rect head{body.x, y, header_w, t.track_height};
        const Rect lane{x0, y, body.right() - x0, t.track_height};
        c.fill_rect(head, i == 1 ? t.track_header_selected : t.track_header);
        c.text({head.x + 10, y, head.w - 40, 20}, names[i], t.text, t.font_size);
        c.fill_rect({head.right() - 26, y + 5, 18, 14}, t.button, 2);
        c.text({head.right() - 26, y + 5, 18, 14}, "M", t.text_dim, t.font_size_small, Align::Center, true);
        c.fill_rect(lane, i % 2 ? t.lane_alt : t.lane);
        c.hline(body.x, body.right(), y + t.track_height - 1, t.panel_border);
    }
    // grid
    c.push_clip({x0, body.y, body.right() - x0, 4 * t.track_height});
    for (int q = 0; q * 30.0f < body.w; ++q) {
        const float x = x0 + q * 30.0f;
        const Color col = q % 4 == 0 ? t.grid_bar : t.grid_beat;
        c.vline(x, body.y, body.y + 4 * t.track_height, col);
        c.vline(x + 15, body.y, body.y + 4 * t.track_height, t.grid_sub);
    }
    // clips: loop references with a miniature of the notes
    auto clip = [&](int track, float bar_from, float bars) {
        const Rect cr{x0 + bar_from * bar_w + 1, body.y + track * t.track_height + 2, bars * bar_w - 2,
                      t.track_height - 5};
        c.fill_rect(cr, t.button, 2);
        c.fill_rect({cr.x, cr.y, cr.w, 14}, t.loop_off, 2);
        c.text({cr.x + 6, cr.y, cr.w - 12, 14}, "Loop", t.text, t.font_size_small, Align::Left, true);
        for (int rep = 0; rep < static_cast<int>(bars / 2); ++rep) {
            const float bx = cr.x + rep * 2 * bar_w;
            if (rep > 0) c.vline(bx, cr.y + 14, cr.bottom(), t.panel_border);
            for (int n = 0; n < 16; ++n) {
                const int sound = n % 4;
                const float ny = cr.y + 18 + sound * 6.0f;
                c.fill_rect({bx + 4 + n * 14.5f, ny, 10, 3}, t.sound_colors[static_cast<size_t>(sound)]);
            }
        }
    };
    clip(0, 0, 4);
    clip(0, 4, 4);
    clip(1, 2, 2);
    clip(2, 4, 4);
    c.pop_clip();
    // playhead
    const float ph = x0 + bar_w * 1.55f;
    c.vline(ph, ruler.y + t.loop_strip_height, body.y + 4 * t.track_height, t.playhead);
}

void channel_rack(Canvas& c, const Theme& t, Rect r) {
    c.fill_rect(r, t.panel);
    Rect body = r;
    panel_header(c, t, cut_top(body, t.panel_header_height), "Channel Rack");
    struct Ch { const char* name; bool on; int color; float vol; float pan; };
    const Ch rows[] = {{"Kick", true, 0, 0.8f, 0.5f},  {"Snare", true, 1, 0.7f, 0.5f},
                       {"Hat", true, 4, 0.6f, 0.62f}, {"Open Hat", false, 5, 0.55f, 0.4f},
                       {"808", true, 6, 0.85f, 0.5f}, {"Perc", false, 8, 0.5f, 0.7f}};
    float y = body.y + 4;
    int i = 0;
    for (const Ch& ch : rows) {
        const Rect row{body.x, y, body.w, 28};
        if (i == 2) c.fill_rect(row, t.track_header_selected);
        c.fill_circle({row.x + 14, y + 14}, 4.5f, ch.on ? t.accent : t.button_hover);
        c.fill_rect({row.x + 26, y + 6, 4, 16}, t.sound_colors[static_cast<size_t>(ch.color)]);
        c.text({row.x + 38, y, row.w - 110, 28}, ch.name, ch.on ? t.text : t.text_dim, t.font_size);
        knob(c, t, {row.right() - 52, y + 14}, ch.vol);
        knob(c, t, {row.right() - 24, y + 14}, ch.pan);
        y += 30;
        ++i;
    }
}

void log_panel(Canvas& c, const Theme& t, Rect r) {
    c.fill_rect(r, t.panel);
    Rect body = r;
    const Rect head = cut_top(body, t.panel_header_height);
    panel_header(c, t, head, "Log");
    button(c, t, {head.right() - 48, head.y + 3, 42, head.h - 6}, "Clear", false);
    struct Line { const char* time; const char* msg; int level; };
    const Line lines[] = {{"12:04:31", "Audio: 48000 Hz, 480 samples", 0},
                          {"12:04:40", "Loaded kick_808_hard.wav", 0},
                          {"12:05:02", "Missing sample: vox_chop.wav", 1},
                          {"12:05:10", "Saved untitled.aloglas", 0},
                          {"12:05:44", "Export failed: file is in use", 2}};
    float y = body.y + 4;
    for (const Line& l : lines) {
        c.text({body.x + 8, y, 56, 18}, l.time, t.text_dim, t.font_size_small);
        const Color col = l.level == 2 ? t.error : (l.level == 1 ? t.warning : t.text);
        c.text({body.x + 66, y, body.w - 74, 18}, l.msg, col, t.font_size_small);
        y += 18;
    }
}

void piano_roll(Canvas& c, const Theme& t, Rect r) {
    c.fill_rect(r, t.panel);
    Rect body = r;
    const Rect head = cut_top(body, t.panel_header_height + 6);
    c.fill_rect(head, t.panel_header);
    c.hline(head.x, head.right(), head.bottom() - 1, t.panel_border);
    float x = head.x + 8;
    const float hy = head.y + 3;
    const float hh = head.h - 6;
    button(c, t, {x, hy, 46, hh}, "Keys", true);
    button(c, t, {x + 47, hy, 50, hh}, "Drums", false);
    x += 110;
    c.text({x, hy, 46, hh}, "Sound", t.text_dim, t.font_size_small);
    c.fill_rect({x + 42, hy, 90, hh}, t.field_bg, 2);
    c.fill_rect({x + 48, hy + 5, 4, hh - 10}, t.sound_colors[6]);
    c.text({x + 58, hy, 60, hh}, "808", t.text, t.font_size_small, Align::Left, true);
    tri_down(c, {x + 118, hy + hh / 2}, t.text_dim);
    x += 146;
    field(c, t, {x, hy, 62, hh}, "Root C5", false);
    field(c, t, {x + 66, hy, 52, hh}, "Fine 0", false);
    button(c, t, {x + 124, hy, 70, hh}, "Cut itself", true);
    button(c, t, {x + 198, hy, 42, hh}, "Gate", true);
    field(c, t, {x + 246, hy, 84, hh}, "Glide 80 ms", false);
    field(c, t, {x + 336, hy, 84, hh}, "Length Auto", false);
    button(c, t, {x + 426, hy, 48, hh}, "Roll", false);
    // drag handle
    const float gx = head.right() - 70;
    for (int i = 0; i < 3; ++i) {
        c.fill_circle({gx, hy + 5 + i * 5.0f}, 1.3f, t.text_dim);
        c.fill_circle({gx + 5, hy + 5 + i * 5.0f}, 1.3f, t.text_dim);
    }
    c.text({gx + 12, hy, 50, hh}, "Loop", t.text, t.font_size_small, Align::Left, true);

    // keyboard + grid + velocity lane
    const float vel_h = 52;
    Rect vel = cut_bottom(body, vel_h);
    const Rect keys = cut_left(body, 54);
    const float row_h = 11;
    const int rows = static_cast<int>(body.h / row_h) + 1;
    const int top_pitch = 64;  // E5 at the top
    for (int i = 0; i < rows; ++i) {
        const int pitch = top_pitch - i;
        const int pc = ((pitch % 12) + 12) % 12;
        const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        const float y = body.y + i * row_h;
        c.fill_rect({keys.x, y, keys.w, row_h}, black ? rgb(0x161616) : rgb(0xBDBDBD));
        c.hline(keys.x, keys.right(), y + row_h - 1, rgb(0x5A5A5A));
        c.fill_rect({body.x, y, body.w, row_h}, black ? t.lane : t.lane_alt);
        if (pc == 0) {
            c.text({keys.x + 24, y, 28, row_h}, "C" + std::to_string(pitch / 12), rgb(0x1A1A1A), 9);
            c.hline(body.x, body.right(), y + row_h - 1, t.grid_bar);
        }
    }
    c.push_clip(body);
    const float q = 44;  // px per quarter
    for (int i = 0; i * q / 4 < body.w; ++i) {
        const float gxl = body.x + i * q / 4;
        const Color col = i % 16 == 0 ? t.grid_bar : (i % 4 == 0 ? t.grid_beat : t.grid_sub);
        c.vline(gxl, body.y, body.bottom(), col);
    }
    auto note_y = [&](int pitch) { return body.y + (top_pitch - pitch) * row_h; };
    // ghost notes of other sounds (drums)
    for (int i = 0; i < 16; ++i) {
        c.fill_rect({body.x + i * q / 2 + 1, note_y(60) + 1, q / 2 - 3, row_h - 2}, t.button_hover, 1);
    }
    // 808 notes with a slide
    struct N { float start; float len; int pitch; bool slide; };
    const N notes[] = {{0, 3, 55, false}, {3, 1, 58, true}, {4, 2, 53, false}, {6, 2, 55, false},
                       {8, 3, 55, false}, {11, 1, 50, true}, {12, 4, 53, false}};
    const Color sc = t.sound_colors[6];
    for (const N& n : notes) {
        const Rect nr{body.x + n.start * q + 1, note_y(n.pitch) + 1, n.len * q - 2, row_h - 2};
        c.fill_rect(nr, n.start == 4 ? t.accent : sc, 2);
        if (n.slide) c.fill_triangle({nr.x + 2, nr.bottom() - 1}, {nr.x + 8, nr.y + 1}, {nr.x + 8, nr.bottom() - 1}, t.text);
    }
    c.vline(body.x + q * 5.4f, body.y, body.bottom(), t.playhead);
    c.pop_clip();

    c.fill_rect(vel, t.panel_header);
    c.hline(vel.x, vel.right(), vel.y, t.panel_border);
    c.text({vel.x + 6, vel.y, 50, 16}, "Velocity", t.text_dim, 9);
    for (const N& n : notes) {
        const float v = 0.55f + 0.06f * static_cast<float>(static_cast<int>(n.start) % 5);
        const float bx = body.x + n.start * q + 1;
        const float bh = (vel.h - 10) * v;
        c.vline(bx, vel.bottom() - 4 - bh, vel.bottom() - 4, sc);
        c.fill_rect({bx - 2, vel.bottom() - 4 - bh - 2, 5, 4}, sc, 1);
    }
    // focused panel: thin accent border
    c.stroke_rect(r, t.accent, 1);
}

}  // namespace

void PalettePreview::draw(Canvas& c) {
    const Theme& t = theme();
    c.fill_rect(rect, t.background);
    Rect area = rect;
    transport(c, t, cut_top(area, t.transport_height));
    const Rect status = cut_bottom(area, t.status_height);
    const Rect left = cut_left(area, 220);
    browser(c, t, left);
    c.vline(left.right(), left.y, left.bottom(), t.panel_border);

    Rect top = area;
    const Rect bottom = cut_bottom(top, std::round(area.h * 0.42f));
    const Rect right = cut_right(top, 280);
    playlist(c, t, top);
    c.vline(right.x, right.y, right.bottom(), t.panel_border);
    Rect rc = right;
    const Rect log_r = cut_bottom(rc, std::round(right.h * 0.4f));
    channel_rack(c, t, rc);
    c.hline(log_r.x, log_r.right(), log_r.y, t.panel_border);
    log_panel(c, t, log_r);
    c.hline(bottom.x, bottom.right(), bottom.y, t.panel_border);
    piano_roll(c, t, bottom);

    c.fill_rect(status, t.panel_header);
    c.hline(status.x, status.right(), status.y, t.panel_border);
    c.text({status.x + 10, status.y, 500, status.h}, "Audio: 48000 Hz, 480 samples  \xC2\xB7  untitled.aloglas *",
           t.text_dim, t.font_size_small);
    c.text({status.right() - 410, status.y, 400, status.h},
           "Palette preview: " + t.name + "   (keys 1 / 2 / 3)", t.text_dim, t.font_size_small, Align::Right);
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
