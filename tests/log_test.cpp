#include <doctest.h>

#include <string>

#include "app/log.h"
#include "app/log_panel.h"
#include "support/fake_host.h"

using namespace aloglas;
using namespace aloglas::test;

TEST_CASE("log keeps the last 500 lines") {
    Log log;
    for (int i = 0; i < 10'000; ++i) log.add_at("12:00:00", LogLevel::Info, std::to_string(i));
    CHECK(log.lines().size() == 500);
    CHECK(log.lines().front().text == "9500");
    CHECK(log.lines().back().text == "9999");
    CHECK(log.total_added() == 10'000);
}

TEST_CASE("log add stamps hh:mm:ss and notifies") {
    Log log;
    int changes = 0;
    log.on_change = [&] { ++changes; };
    log.add(LogLevel::Warning, "hello");
    REQUIRE(log.lines().size() == 1);
    CHECK(log.lines()[0].time.size() == 8);
    CHECK(log.lines()[0].time[2] == ':');
    CHECK(log.lines()[0].level == LogLevel::Warning);
    CHECK(changes == 1);
}

TEST_CASE("log panel sticks to the bottom until scrolled up") {
    Log log;
    for (int i = 0; i < 50; ++i) log.add_at("12:00:00", LogLevel::Info, std::to_string(i));
    LogPanel panel(log);
    EventRouter router(panel);
    FakeHost host(router);
    panel.set_host(&host);
    panel.layout({0, 0, 300, 100});  // 5 rows of 17 px
    CHECK(panel.scroll_from_bottom() == 0);

    router.wheel({{50, 50}, 0, 1});  // one notch away from the user: older lines
    CHECK(panel.scroll_from_bottom() == 3);
    for (int i = 0; i < 5; ++i) log.add_at("12:00:01", LogLevel::Info, "new");
    panel.layout(panel.rect);
    CHECK(panel.scroll_from_bottom() == 8);  // the same lines stay in view

    router.wheel({{50, 50}, 0, -100});
    CHECK(panel.scroll_from_bottom() == 0);
    log.add_at("12:00:02", LogLevel::Info, "newest");
    panel.layout(panel.rect);
    CHECK(panel.scroll_from_bottom() == 0);

    router.wheel({{50, 50}, 0, 100});  // clamped to lines - visible rows
    CHECK(panel.scroll_from_bottom() == 56 - 5);
}

TEST_CASE("clear button empties the log") {
    Log log;
    log.add_at("12:00:00", LogLevel::Error, "x");
    LogPanel panel(log);
    REQUIRE(panel.clear_button().on_click);
    panel.clear_button().on_click();
    CHECK(log.lines().empty());
}
