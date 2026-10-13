#include <doctest.h>

#include <thread>

#include "core/spsc_queue.h"

using namespace aloglas;

TEST_CASE("SpscQueue holds exactly Capacity items, FIFO") {
    SpscQueue<int, 4> q;
    for (int i = 0; i < 4; ++i) CHECK(q.push(i));
    CHECK_FALSE(q.push(99));

    int v = -1;
    for (int i = 0; i < 4; ++i) {
        REQUIRE(q.pop(v));
        CHECK(v == i);
    }
    CHECK_FALSE(q.pop(v));

    // Indices keep growing past Capacity; slots wrap around.
    for (int round = 0; round < 10; ++round) {
        CHECK(q.push(round));
        REQUIRE(q.pop(v));
        CHECK(v == round);
    }
}

TEST_CASE("SpscQueue across threads keeps order") {
    SpscQueue<int, 64> q;
    constexpr int kCount = 1'000'000;

    std::thread producer([&] {
        for (int i = 0; i < kCount; ++i) {
            while (!q.push(i)) std::this_thread::yield();
        }
    });

    int expected = 0;
    int v = 0;
    bool in_order = true;
    while (expected < kCount) {
        if (q.pop(v)) {
            if (v != expected) in_order = false;
            ++expected;
        } else {
            std::this_thread::yield();
        }
    }
    producer.join();
    CHECK(in_order);
    CHECK_FALSE(q.pop(v));
}
