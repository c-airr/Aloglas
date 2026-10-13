#include <doctest.h>

#include "ui/text_edit.h"

using namespace aloglas;

TEST_CASE("TextEdit moves and deletes by code point") {
    TextEdit t;
    t.set_text("żó");
    CHECK(t.caret() == t.text().size());
    CHECK_FALSE(t.has_selection());
    t.backspace();
    CHECK(t.text() == "ż");
    t.move_left(false);
    CHECK(t.caret() == 0);
    t.del();
    CHECK(t.text().empty());
    t.backspace();  // nothing to delete: no crash
    CHECK(t.text().empty());
}

TEST_CASE("TextEdit selection replace and max length") {
    TextEdit t(5);
    t.set_text("abc");
    t.select_all();
    CHECK(t.selected_text() == "abc");
    t.insert("xy");
    CHECK(t.text() == "xy");
    t.insert("1234567");
    CHECK(t.text() == "xy123");             // truncated at 5 code points
    t.home(false);
    t.move_right(true);
    CHECK(t.selected_text() == "x");
    t.backspace();                           // deletes the selection only
    CHECK(t.text() == "y123");
}

TEST_CASE("TextEdit shift selection and caret placement") {
    TextEdit t;
    t.set_text("Bass żółw");
    t.end(false);
    t.move_left(true);
    t.move_left(true);
    CHECK(t.selected_text() == "łw");
    t.set_caret(0, false);
    t.end(true);
    CHECK(t.selected_text() == "Bass żółw");
    t.insert("Kick");
    CHECK(t.text() == "Kick");
}

TEST_CASE("TextEdit max length counts code points, not bytes") {
    TextEdit t(3);
    t.insert("żółw");
    CHECK(t.text() == "żół");
}
