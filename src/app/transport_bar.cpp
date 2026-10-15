#include "app/transport_bar.h"

#include <cmath>
#include <string>

#include "app/frame_update.h"
#include "ui/canvas.h"
#include "ui/theme.h"

namespace aloglas {

TransportBar::TransportBar(Controller& controller, const LiveState& live)
    : controller_(controller),
      live_(live),
      tempo_(NumberSpec{20, 999, 2, 0.5, 0.01}),
      bpm_([] { return std::string("BPM"); }),
      sig_num_(NumberSpec{1, 32, 0, 1.0 / 8, 1.0 / 8}),
      slash_([] { return std::string("/"); }, Canvas::Align::Center),
      sig_den_(NumberSpec{1, 16, 0, 1.0 / 8, 1.0 / 8, {1, 2, 4, 8, 16}}),
      position_([this] { return position_text(live_, controller_.doc()); }, Canvas::Align::Left, true),
      cpu_([this] { return "CPU " + std::to_string(std::lround(live_.cpu_load * 100)) + "%"; }),
      meter_([this] { return live_.meter_left; }, [this] { return live_.meter_right; }) {
    play_.value = [this] { return live_.playing; };
    play_.on_toggle = [this](bool) { controller_.toggle_play(live_.playing); };
    play_.draw_icon = [](Canvas& c, Rect r, bool on) {
        const float cx = r.x + r.w / 2;
        const float cy = r.y + r.h / 2;
        c.fill_triangle({cx - 3, cy - 4}, {cx + 4, cy}, {cx - 3, cy + 4}, on ? theme().accent_text : theme().text);
    };
    stop_.on_click = [this] { controller_.stop(); };

    tempo_.value = [this] { return controller_.doc().tempo; };
    tempo_.on_change = [this](double v) { controller_.set_tempo(v); };
    sig_num_.value = [this] { return double(controller_.doc().signature.num); };
    sig_num_.on_change = [this](double v) {
        controller_.set_signature({static_cast<int>(v), controller_.doc().signature.den});
    };
    sig_den_.value = [this] { return double(controller_.doc().signature.den); };
    sig_den_.on_change = [this](double v) {
        controller_.set_signature({controller_.doc().signature.num, static_cast<int>(v)});
    };

    loop_mode_.value = [this] { return controller_.doc().play_mode == PlayMode::Loop; };
    loop_mode_.on_toggle = [this](bool) { controller_.set_play_mode(PlayMode::Loop); };
    song_mode_.value = [this] { return controller_.doc().play_mode == PlayMode::Song; };
    song_mode_.on_toggle = [this](bool) { controller_.set_play_mode(PlayMode::Song); };
    metronome_.value = [this] { return controller_.doc().metronome; };
    metronome_.on_toggle = [this](bool on) { controller_.set_metronome(on); };
    brace_.value = [this] { return controller_.doc().brace.enabled; };
    brace_.on_toggle = [this](bool on) { controller_.set_brace_enabled(on); };
    follow_.value = [this] { return controller_.doc().follow; };
    follow_.on_toggle = [this](bool on) { controller_.set_follow(on); };

    for (Widget* w : std::initializer_list<Widget*>{&play_, &stop_, &tempo_, &bpm_, &sig_num_, &slash_, &sig_den_,
                                                    &loop_mode_, &song_mode_, &metronome_, &brace_, &follow_,
                                                    &position_, &cpu_, &meter_}) {
        add(*w);
    }
}

void TransportBar::layout(Rect r) {
    rect = r;
    const Theme& t = theme();
    const float h = t.control_height;
    const float y = r.y + std::floor((r.h - h) / 2);
    float x = r.x + 8;
    // Members of a group touch: the next one starts on the previous one's
    // last pixel column, so they share a single 1-px border.
    auto place = [&](Widget& w, float width) {
        w.layout({x, y, width, h});
        x += width - 1;
    };
    auto end_group = [&] { x += 1 + t.group_gap; };

    place(play_, 28);
    place(stop_, 28);
    end_group();
    place(tempo_, 52);
    x += 1;
    bpm_.layout({x + 4, y, 24, h});
    x += 28 - 1;
    end_group();
    place(sig_num_, 22);
    x += 1;
    slash_.layout({x, y, 10, h});
    x += 10;
    place(sig_den_, 22);
    end_group();
    place(loop_mode_, 46);
    place(song_mode_, 46);
    end_group();
    place(metronome_, 46);
    place(brace_, 46);
    place(follow_, 50);
    end_group();
    position_.layout({x + 8, y, 150 - 16, h});

    meter_.layout({r.right() - 110, y + 3, 100, h - 6});
    cpu_.layout({r.right() - 176, y, 60, h});
}

void TransportBar::draw(Canvas& c) {
    const Theme& t = theme();
    c.fill_rect(rect, t.panel);
    c.hline(rect.x, rect.right(), rect.bottom() - 1, t.panel_border);
    // The position label sits in a dark field box.
    const Rect pos{position_.rect.x - 8, position_.rect.y, position_.rect.w + 16, position_.rect.h};
    c.fill_rect(pos, t.field_bg);
    c.stroke_rect(pos, t.panel_border);
    draw_children(c);
    // Stop square on top of the plain stop button.
    const Rect s = stop_.rect;
    c.fill_rect({std::round(s.x + s.w / 2 - 3.5f), std::round(s.y + s.h / 2 - 3.5f), 7, 7}, t.text);
}

}  // namespace aloglas
