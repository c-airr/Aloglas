#include <doctest.h>

TEST_CASE("toolchain smoke") {
    CHECK(sizeof(void*) == 8);
    CHECK(__cplusplus >= 202002L);
}
