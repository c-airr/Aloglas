// The bar at the top: play/stop, tempo, time signature, LOOP/SONG, metronome,
// loop brace, follow, position, CPU and the master meter.
#pragma once

#include "app/controller.h"
#include "app/live_state.h"
#include "ui/widget.h"
#include "ui/widgets/button.h"
#include "ui/widgets/label.h"
#include "ui/widgets/meter_bar.h"
#include "ui/widgets/number_box.h"
#include "ui/widgets/toggle_button.h"

namespace aloglas {

class TransportBar : public Widget {
public:
    TransportBar(Controller& controller, const LiveState& live);

    void layout(Rect r) override;
    void draw(Canvas& c) override;

    ToggleButton& play() { return play_; }
    Button& stop() { return stop_; }
    NumberBox& tempo() { return tempo_; }
    NumberBox& sig_num() { return sig_num_; }
    NumberBox& sig_den() { return sig_den_; }
    ToggleButton& loop_mode() { return loop_mode_; }
    ToggleButton& song_mode() { return song_mode_; }
    ToggleButton& metronome() { return metronome_; }
    ToggleButton& brace() { return brace_; }
    ToggleButton& follow() { return follow_; }
    Label& position() { return position_; }
    Label& cpu_label() { return cpu_; }

private:
    Controller& controller_;
    const LiveState& live_;

    ToggleButton play_{""};
    Button stop_{""};
    NumberBox tempo_;
    Label bpm_;
    NumberBox sig_num_;
    Label slash_;
    NumberBox sig_den_;
    ToggleButton loop_mode_{"LOOP"};
    ToggleButton song_mode_{"SONG"};
    ToggleButton metronome_{"Metro"};
    ToggleButton brace_{"Brace"};
    ToggleButton follow_{"Follow"};
    Label position_;
    Label cpu_;
    MeterBar meter_;
};

}  // namespace aloglas
