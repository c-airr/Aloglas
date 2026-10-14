#include <doctest.h>

#include <cmath>

#include "ui/number_format.h"

using namespace aloglas;

TEST_CASE("parse_number accepts comma and clamps") {  // Review Focus 3
    const NumberSpec bpm{20, 999, 2, 0.5, 0.01, {}};
    CHECK(*parse_number(bpm, "140,5") == doctest::Approx(140.5));
    CHECK(*parse_number(bpm, " 95.123 ") == doctest::Approx(95.12));
    CHECK(*parse_number(bpm, "5000") == doctest::Approx(999));
    CHECK(*parse_number(bpm, "3") == doctest::Approx(20));
    CHECK_FALSE(parse_number(bpm, "abc"));
    CHECK_FALSE(parse_number(bpm, ""));
    CHECK_FALSE(parse_number(bpm, "   "));
    CHECK_FALSE(parse_number(bpm, "12abc"));
    // Final review finding 4: from_chars accepts these, a tempo must not.
    CHECK_FALSE(parse_number(bpm, "nan"));
    CHECK_FALSE(parse_number(bpm, "NaN"));
    CHECK_FALSE(parse_number(bpm, "inf"));
    CHECK_FALSE(parse_number(bpm, "-inf"));
}

TEST_CASE("parse_number with allowed values picks the nearest") {
    const NumberSpec den{1, 16, 0, 1.0 / 8, 1.0 / 8, {1, 2, 4, 8, 16}};
    CHECK(*parse_number(den, "8") == 8);
    CHECK(*parse_number(den, "5") == 4);
    CHECK(*parse_number(den, "100") == 16);
}

TEST_CASE("drag_value") {
    const NumberSpec bpm{20, 999, 2, 0.5, 0.01, {}};
    CHECK(drag_value(bpm, 140, 20, false) == doctest::Approx(150));
    CHECK(drag_value(bpm, 140, 20, true) == doctest::Approx(140.2));
    CHECK(drag_value(bpm, 140, -20, false) == doctest::Approx(130));
    CHECK(drag_value(bpm, 990, 100, false) == doctest::Approx(999));

    const NumberSpec den{1, 16, 0, 1.0 / 8, 1.0 / 8, {1, 2, 4, 8, 16}};
    CHECK(drag_value(den, 4, 8, false) == 8);
    CHECK(drag_value(den, 4, -16, false) == 1);
    CHECK(drag_value(den, 16, 80, false) == 16);
}

TEST_CASE("clamp_to_spec and format_number") {
    const NumberSpec bpm{20, 999, 2};
    CHECK(clamp_to_spec(bpm, 140.456) == doctest::Approx(140.46));
    CHECK(clamp_to_spec(bpm, 10) == 20);
    CHECK(clamp_to_spec(bpm, std::nan("")) == 20);  // NaN never leaks out
    CHECK(format_number(bpm, 140) == "140.00");
    CHECK(format_number({1, 32, 0}, 7) == "7");
}
