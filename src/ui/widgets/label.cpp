#include "ui/widgets/label.h"

#include "ui/theme.h"

namespace aloglas {

void Label::draw(Canvas& c) {
    if (text_) c.text(rect, text_(), theme().text, theme().font_size, align_, bold_);
}

}  // namespace aloglas
