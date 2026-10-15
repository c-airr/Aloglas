#include "app/log_panel.h"

#include <algorithm>
#include <cmath>

#include "ui/canvas.h"
#include "ui/theme.h"

namespace aloglas {

LogPanel::LogPanel(Log& log) : log_(log), seen_(log.total_added()) {
    clear_.on_click = [this] { log_.clear(); };
}

int LogPanel::visible_rows() const {
    return std::max(0, static_cast<int>(rect.h / theme().row_height));
}

void LogPanel::sync() {
    const uint64_t total = log_.total_added();
    if (scroll_ > 0) scroll_ += static_cast<int>(total - seen_);
    seen_ = total;
    const int max_scroll = std::max(0, static_cast<int>(log_.lines().size()) - visible_rows());
    scroll_ = std::clamp(scroll_, 0, max_scroll);
}

void LogPanel::layout(Rect r) {
    rect = r;
    sync();
}

bool LogPanel::on_wheel(const WheelEvent& e) {
    sync();
    scroll_ += static_cast<int>(std::lround(e.dy * 3));
    sync();
    invalidate();
    return true;
}

void LogPanel::draw(Canvas& c) {
    sync();
    const Theme& t = theme();
    c.fill_rect(rect, t.panel);
    const auto& lines = log_.lines();
    const int rows = visible_rows();
    const int last = static_cast<int>(lines.size()) - scroll_;  // one past the bottom row
    const int first = std::max(0, last - rows);
    c.push_clip(rect);
    float y = rect.y + 2;
    for (int i = first; i < last; ++i) {
        const LogLine& l = lines[static_cast<std::size_t>(i)];
        const Color col = l.level == LogLevel::Error ? t.error : l.level == LogLevel::Warning ? t.warning : t.text;
        c.text({rect.x + 8, y, 52, t.row_height}, l.time, t.text_dim, t.font_size_small);
        c.text({rect.x + 60, y, rect.w - 66, t.row_height}, l.text, col, t.font_size_small);
        y += t.row_height;
    }
    c.pop_clip();
}

}  // namespace aloglas
