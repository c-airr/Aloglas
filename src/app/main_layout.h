// Where each panel of the main window goes. Pure functions: no widgets, so the
// rules (defaults, minimums, tiny windows) are tested directly.
//
//   +---------------------------------------------+
//   | transport                                   |
//   +---------+-----------------------+-----------+
//   | browser | playlist              | rack      |
//   |         |                       +-----------+
//   |         |                       | log       |
//   |         +-----------------------+-----------+
//   |         | piano roll                        |
//   +---------+-----------------------------------+
//   | status                                      |
//   +---------------------------------------------+
#pragma once

#include "ui/geometry.h"
#include "ui/theme.h"

namespace aloglas {

// What the user changes with the splitters (saved in settings).
struct PanelSizes {
    float browser_width = 220;  // browser column, including its 1-px gap
    float right_width = 280;    // rack/log column
    float rack_share = 0.6f;    // of the right column's height
    float piano_share = 0.4f;   // of the height below the transport
    bool operator==(const PanelSizes&) const = default;
};

struct PanelMinimums {
    float browser = 140;
    float playlist = 240;
    float right = 180;
    float rack = 60;
    float log = 60;
    float top = 120;
    float piano = 120;
};

struct MainLayout {
    Rect transport, status, browser, playlist, rack, log, piano;
    // Grab areas of the splitters, centred on the 1-px gaps between panels.
    Rect split_browser, split_right, split_rack_log, split_top_piano;
};

MainLayout compute_layout(Rect window, const PanelSizes& sizes, const Theme& theme);

// Sizes that leave every neighbour at least its minimum. When the window is
// too small for both, the size goes to the middle of the impossible range
// (never negative); compute_layout then shrinks what doesn't fit.
PanelSizes clamp_sizes(PanelSizes sizes, Rect window, const Theme& theme, PanelMinimums min = {});

}  // namespace aloglas
