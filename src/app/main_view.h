// The root widget of the window: transport, the five panels, the status bar
// and the splitters between the panels. Also handles global shortcuts.
#pragma once

#include "app/browser_panel.h"
#include "app/controller.h"
#include "app/live_state.h"
#include "app/log.h"
#include "app/log_panel.h"
#include "app/main_layout.h"
#include "app/panel.h"
#include "app/status_bar.h"
#include "app/transport_bar.h"
#include "ui/widget.h"
#include "ui/widgets/popup_menu.h"
#include "ui/widgets/splitter.h"

namespace aloglas {

class MainView : public Widget {
public:
    // The browser and the popup live outside (main wires them to the loader
    // and previewer); the view places them.
    MainView(Controller& controller, const LiveState& live, Log& log, BrowserPanel& browser, PopupMenu& popup);

    // The sizes the user chose (saved in settings). The layout uses a
    // clamped copy, so a tiny or minimized window never overwrites them.
    PanelSizes& sizes() { return sizes_; }
    const MainLayout& main_layout() const { return layout_; }
    Panel* focused_panel() const { return focused_; }
    TransportBar& transport() { return transport_; }

    void layout(Rect r) override;
    bool on_key(const KeyEvent& e) override;

private:
    // Splitter changes: store, clamp against the current window, relayout.
    void resize(void (*apply)(PanelSizes&, float), float v);

    Controller& controller_;
    const LiveState& live_;
    PanelSizes sizes_;
    PanelSizes shown_;  // sizes_ clamped to the current window: what the splitters drag from
    MainLayout layout_;
    Panel* focused_ = nullptr;

    TransportBar transport_;
    Panel browser_{"BROWSER", focused_};
    Panel playlist_{"PLAYLIST", focused_};
    Panel rack_{"CHANNEL RACK", focused_};
    Panel log_{"LOG", focused_};
    Panel piano_{"PIANO ROLL", focused_};
    // Bodies of the panels that later steps fill in.
    Widget playlist_body_, rack_body_, piano_body_;
    LogPanel log_body_;
    StatusBar status_;
    Splitter split_browser_;
    Splitter split_right_;
    Splitter split_rack_log_;
    Splitter split_top_piano_;
    PopupMenu& popup_;
};

}  // namespace aloglas
