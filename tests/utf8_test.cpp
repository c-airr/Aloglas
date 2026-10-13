#include <doctest.h>

#include "core/utf8.h"

using namespace aloglas;

TEST_CASE("utf8 helpers") {
    CHECK(utf8_length("") == 0);
    CHECK(utf8_length("abc") == 3);
    CHECK(utf8_length("żółw") == 4);
    CHECK(utf8_truncate("żółwie", 3) == "żół");
    CHECK(utf8_truncate("abc", 10) == "abc");

    const std::string s = "aż";             // 'a' = 1 byte, 'ż' = 2 bytes
    CHECK(utf8_next(s, 0) == 1);
    CHECK(utf8_next(s, 1) == 3);
    CHECK(utf8_next(s, 3) == 3);            // clamped at the end
    CHECK(utf8_prev(s, 3) == 1);
    CHECK(utf8_prev(s, 1) == 0);
    CHECK(utf8_prev(s, 0) == 0);

    CHECK(trim("  a b \t") == "a b");
    CHECK(trim("   ").empty());
}
