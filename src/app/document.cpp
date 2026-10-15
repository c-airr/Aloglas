#include "app/document.h"

namespace aloglas {

int64_t loop_length_ticks(const Document& doc) {
    // shortcut: no notes yet, so Auto = one bar; the piano roll step adds the real Auto/1/2/4/8 rule
    return ticks_per_bar(doc.signature);
}

}  // namespace aloglas
