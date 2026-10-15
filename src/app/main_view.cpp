#include "app/main_view.h"

#include "ui/theme.h"

namespace aloglas {

namespace {
// The heights the two row splitters' shares are taken from.
float right_column_height(const MainLayout& l) {
    return l.rack.h + 1 + l.log.h;
}
float below_transport_height(const MainLayout& l) {
    return l.browser.h;
}
float share(float px, float total) {
    return total > 0 ? px / total : 0.0f;
}
}  // namespace

MainView::MainView(Controller& controller, const LiveState& live, Log& log, BrowserPanel& browser,
                   PopupMenu& popup)
    : controller_(controller),
      live_(live),
      transport_(controller, live),
      log_body_(log),
      status_(live),
      split_browser_(
          SplitAxis::Columns, [this] { return shown_.browser_width; },
          [this](float v) { resize([](PanelSizes& s, float x) { s.browser_width = x; }, v); }, +1,
          [this] { resize([](PanelSizes& s, float) { s.browser_width = PanelSizes{}.browser_width; }, 0); }),
      split_right_(
          SplitAxis::Columns, [this] { return shown_.right_width; },
          [this](float v) { resize([](PanelSizes& s, float x) { s.right_width = x; }, v); }, -1,
          [this] { resize([](PanelSizes& s, float) { s.right_width = PanelSizes{}.right_width; }, 0); }),
      // The two row splitters drag in pixels but store shares of a height.
      split_rack_log_(
          SplitAxis::Rows, [this] { return shown_.rack_share * right_column_height(layout_); },
          [this](float v) {
              resize([](PanelSizes& s, float x) { s.rack_share = x; }, share(v, right_column_height(layout_)));
          },
          +1, [this] { resize([](PanelSizes& s, float) { s.rack_share = PanelSizes{}.rack_share; }, 0); }),
      split_top_piano_(
          SplitAxis::Rows, [this] { return shown_.piano_share * below_transport_height(layout_); },
          [this](float v) {
              resize([](PanelSizes& s, float x) { s.piano_share = x; }, share(v, below_transport_height(layout_)));
          },
          -1, [this] { resize([](PanelSizes& s, float) { s.piano_share = PanelSizes{}.piano_share; }, 0); }),
      popup_(popup) {
    browser_.set_body(browser);
    // Right to left: Preview at the edge, + Folder before it.
    browser_.add_header_widget(browser.preview_button(), 52);
    browser_.add_header_widget(browser.add_folder_button(), 58);
    playlist_.set_body(playlist_body_);
    rack_.set_body(rack_body_);
    log_.set_body(log_body_);
    log_.add_header_widget(log_body_.clear_button(), 40);
    piano_.set_body(piano_body_);

    add(transport_);
    add(browser_);
    add(playlist_);
    add(rack_);
    add(log_);
    add(piano_);
    add(status_);
    // Splitters last: they overlap the panel edges and must win hit-tests.
    add(split_browser_);
    add(split_right_);
    add(split_rack_log_);
    add(split_top_piano_);
    add(popup_);  // on top of everything while open
}

void MainView::resize(void (*apply)(PanelSizes&, float), float v) {
    apply(sizes_, v);
    sizes_ = clamp_sizes(sizes_, rect, theme());
    layout(rect);
    invalidate();
}

void MainView::layout(Rect r) {
    rect = r;
    shown_ = clamp_sizes(sizes_, r, theme());
    layout_ = compute_layout(r, shown_, theme());
    transport_.layout(layout_.transport);
    browser_.layout(layout_.browser);
    playlist_.layout(layout_.playlist);
    rack_.layout(layout_.rack);
    log_.layout(layout_.log);
    piano_.layout(layout_.piano);
    status_.layout(layout_.status);
    split_browser_.layout(layout_.split_browser);
    split_right_.layout(layout_.split_right);
    split_rack_log_.layout(layout_.split_rack_log);
    split_top_piano_.layout(layout_.split_top_piano);
    popup_.layout(r);
}

bool MainView::on_key(const KeyEvent& e) {
    if (e.key == Key::Space && !e.mods.ctrl && !e.mods.alt) {
        controller_.toggle_play(live_.playing);
        return true;
    }
    if (e.key == Key::L && e.mods.ctrl) {
        controller_.toggle_brace();
        return true;
    }
    return false;
}

}  // namespace aloglas
