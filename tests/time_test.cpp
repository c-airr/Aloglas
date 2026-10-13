#include <doctest.h>

#include "core/time.h"

using namespace aloglas;

TEST_CASE("ticks per beat/bar") {
    CHECK(ticks_per_beat({4, 4}) == 960);
    CHECK(ticks_per_bar({4, 4}) == 3840);
    CHECK(ticks_per_beat({6, 8}) == 480);
    CHECK(ticks_per_bar({6, 8}) == 2880);
    CHECK(ticks_per_beat({7, 4}) == 960);
    CHECK(ticks_per_bar({7, 4}) == 6720);
    CHECK(max_ticks({4, 4}) == 3'840'000);
}

TEST_CASE("signature validity") {
    CHECK(is_valid({4, 4}));
    CHECK(is_valid({32, 16}));
    CHECK(is_valid({1, 1}));
    CHECK_FALSE(is_valid({4, 3}));
    CHECK_FALSE(is_valid({0, 4}));
    CHECK_FALSE(is_valid({33, 4}));
}

TEST_CASE("ticks_per_sample") {
    CHECK(ticks_per_sample(120, 48000) == doctest::Approx(0.04));
    CHECK(ticks_per_sample(140, 48000) == doctest::Approx(140.0 * 960 / (60.0 * 48000)));
}

TEST_CASE("to_bbs") {
    CHECK(format_bbs(to_bbs(0, {4, 4})) == "1.1.1");
    CHECK(format_bbs(to_bbs(3840 + 960 + 240, {4, 4})) == "2.2.2");
    CHECK(format_bbs(to_bbs(2880 + 480 * 5 + 240, {6, 8})) == "2.6.2");
    CHECK(format_bbs(to_bbs(6720 - 1, {7, 4})) == "1.7.4");
    CHECK(to_bbs(3840.0 * 999, {4, 4}).bar == 1000);
}

TEST_CASE("format_clock") {
    CHECK(format_clock(0) == "00:00.000");
    CHECK(format_clock(65.25) == "01:05.250");
    CHECK(format_clock(7500) == "125:00.000");
    CHECK(ticks_to_seconds(1920, 120) == doctest::Approx(1.0));
}

TEST_CASE("first_beat_at_or_after") {
    CHECK(first_beat_at_or_after(0, {4, 4}) == 0);
    CHECK(first_beat_at_or_after(0.01, {4, 4}) == 960);
    CHECK(first_beat_at_or_after(960, {4, 4}) == 960);
    CHECK(first_beat_at_or_after(500, {6, 8}) == 960);
}
