#include <doctest.h>

#include "core/utf8.h"
#include "ui/text_fit.h"

using namespace aloglas;

namespace {
// Fake font: every code point (including the ellipsis) is 10 px wide.
float measure(std::string_view s) {
    return 10.0f * static_cast<float>(utf8_length(s));
}
}  // namespace

TEST_CASE("fit_text keeps text that fits") {
    CHECK(fit_text("Kick", 40, measure) == "Kick");
    CHECK(fit_text("", 0, measure).empty());
}

TEST_CASE("fit_text cuts by code point and adds an ellipsis") {
    CHECK(fit_text("Kick 808 long", 50, measure) == "Kick…");      // 4 chars + … = 50 px
    CHECK(fit_text("żółwie", 30, measure) == "żó…");
}

TEST_CASE("fit_text returns nothing when not even the ellipsis fits") {
    CHECK(fit_text("Kick", 5, measure).empty());
    CHECK(fit_text("Kick", 10, measure) == "…");
}
