#include <doctest.h>

#include "core/natural_sort.h"

using namespace aloglas;

TEST_CASE("natural order") {
    CHECK(natural_less("Kick 2", "Kick 10"));
    CHECK_FALSE(natural_less("Kick 10", "Kick 2"));
    CHECK(natural_less("kick", "Snare"));
    CHECK(natural_less("Kick", "snare"));
    CHECK(natural_less("a1", "a01"));  // equal values: shorter digit run first
    CHECK_FALSE(natural_less("a01", "a1"));
    CHECK(natural_less("808 a", "808 b"));
    CHECK_FALSE(natural_less("x", "x"));
    CHECK(natural_less("x", "x1"));
    CHECK(natural_less("track 9 b", "track 10 a"));
    CHECK(natural_less("v99999999999999999999 a", "v99999999999999999999 b"));  // no integer overflow
}
