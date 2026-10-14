// AudioDevice with miniaudio's null backend: a simulated device that pulls
// audio in real time without touching any hardware.
#include <doctest.h>

#include <chrono>
#include <thread>

#include "audio/audio_device.h"
#include "engine/engine.h"

using namespace aloglas;
using namespace std::chrono_literals;

TEST_CASE("null device runs the engine") {
    Engine e;
    AudioDevice d(e, {.null_backend = true});
    CHECK(d.status() == AudioStatus::Closed);
    REQUIRE(d.open());
    CHECK(d.status() == AudioStatus::Running);
    CHECK(d.sample_rate() > 0);
    CHECK(d.buffer_frames() > 0);
    CHECK(d.open());  // idempotent while running

    std::this_thread::sleep_for(200ms);
    CHECK(e.frames_rendered() > 0);
    CHECK(d.cpu_load() >= 0.0f);

    d.close();
    CHECK(d.status() == AudioStatus::Closed);
    CHECK_FALSE(d.take_lost());  // a stop we asked for is not a lost device
}

TEST_CASE("engine settings survive close and reopen") {
    Engine e;
    AudioDevice d(e, {.null_backend = true});
    REQUIRE(d.open());
    e.send(Command::play());
    std::this_thread::sleep_for(100ms);
    e.send(Command::set_tempo(90));
    d.close();
    e.pump();  // no audio thread now: apply what is queued
    const double pos_closed = e.position_ticks();
    CHECK(e.playing());

    REQUIRE(d.open());
    std::this_thread::sleep_for(100ms);
    CHECK(e.position_ticks() > pos_closed);
    CHECK(e.dropped_commands() == 0);
    d.close();
}

// Final review finding 2: miniaudio follows the default output by stopping
// and restarting the stream; that is not a lost device. A stream that stays
// stopped is, and open() must bring it back.
TEST_CASE("a brief stream stop (output rerouted) is not a lost device") {
    Engine e;
    AudioDevice d(e, {.null_backend = true});
    REQUIRE(d.open());
    std::this_thread::sleep_for(100ms);
    d.debug_stop_stream();
    std::this_thread::sleep_for(50ms);
    d.debug_start_stream();
    std::this_thread::sleep_for(1000ms);
    CHECK_FALSE(d.take_lost());
    d.close();
}

TEST_CASE("a stream that stays stopped is reported lost once, and open() revives it") {
    Engine e;
    AudioDevice d(e, {.null_backend = true});
    REQUIRE(d.open());
    std::this_thread::sleep_for(100ms);
    d.debug_stop_stream();
    std::this_thread::sleep_for(1000ms);
    CHECK(d.take_lost());
    CHECK_FALSE(d.take_lost());  // reported once

    const int64_t frames = e.frames_rendered();
    REQUIRE(d.open());
    std::this_thread::sleep_for(200ms);
    CHECK(e.frames_rendered() > frames);
    CHECK_FALSE(d.take_lost());
    d.close();
}
