#include "app/status_bar.h"

#include "app/frame_update.h"
#include "ui/canvas.h"
#include "ui/theme.h"

namespace aloglas {

void StatusBar::draw(Canvas& c) {
    const Theme& t = theme();
    c.fill_rect(rect, t.panel_header);
    c.hline(rect.x, rect.right(), rect.y, t.panel_border);
    c.text({rect.x + 8, rect.y, rect.w - 16, rect.h}, status_text(live_), t.text_dim, t.font_size_small);
}

}  // namespace aloglas
