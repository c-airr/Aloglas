#include <doctest.h>

#include <chrono>
#include <thread>

#include "app/controller.h"
#include "app/frame_update.h"
#include "app/log.h"
#include "audio/audio_device.h"
#include "engine/engine.h"

using namespace aloglas;
using namespace std::chrono_literals;

namespace {
int count_level(const Log& log, LogLevel level) {
    int n = 0;
    for (const LogLine& l : log.lines()) n += l.level == level;
    return n;
}
}  // namespace

TEST_CASE("status_text") {
    LiveState s;
    s.audio = AudioStatus::Running;
    s.sample_rate = 48000;
    s.buffer_frames = 480;
    CHECK(status_text(s) == "Audio: 48000 Hz, 480 samples");
    s.audio = AudioStatus::NoDevice;
    CHECK(status_text(s) == "No audio \xE2\x80\x94 press Play to retry");
    s.audio = AudioStatus::Closed;
    CHECK(status_text(s) == "No audio \xE2\x80\x94 press Play to retry");
    s.audio = AudioStatus::Error;
    s.audio_error = "busy";
    CHECK(status_text(s) == "Audio error: busy");
    s.audio_disabled = true;
    s.dropped_commands = 3;
    CHECK(status_text(s) == "Audio disabled (--no-audio) \xC2\xB7 3 commands dropped");
}

TEST_CASE("position_text") {
    LiveState s;
    s.position_ticks = 3840 + 960;
    Document d;
    d.tempo = 120;
    CHECK(position_text(s, d) == "2.2.1  00:02.500");
}

TEST_CASE("frame_update pumps engine without device") {
    Engine e;
    e.set_assert_on_drop(false);
    Document d;
    Controller c(d, e);
    Log log;
    LiveState live;
    for (int round = 0; round < 10; ++round) {
        for (int i = 0; i < 100; ++i) c.set_tempo(100 + i + round);
        frame_update(e, nullptr, c, log, live);
    }
    CHECK(live.dropped_commands == 0);
    CHECK(live.audio_disabled);
    CHECK(count_level(log, LogLevel::Warning) == 0);
}

TEST_CASE("dropped commands are logged once per increase") {
    Engine e;
    e.set_assert_on_drop(false);
    Document d;
    Controller c(d, e);
    Log log;
    LiveState live;
    AudioDevice dev(e, {.null_backend = true});  // closed: frame_update pumps, but only once per frame
    for (int i = 0; i < 300; ++i) c.set_tempo(100 + i * 0.01);
    frame_update(e, &dev, c, log, live);
    CHECK(live.dropped_commands > 0);
    CHECK(count_level(log, LogLevel::Warning) == 1);
    frame_update(e, &dev, c, log, live);
    CHECK(count_level(log, LogLevel::Warning) == 1);
}

TEST_CASE("lost device stops and logs once") {
    Engine e;
    Document d;
    Controller c(d, e);
    Log log;
    LiveState live;
    AudioDevice dev(e, {.null_backend = true});
    REQUIRE(dev.open());
    c.play();
    std::this_thread::sleep_for(100ms);
    dev.debug_stop_stream();
    std::this_thread::sleep_for(1000ms);
    frame_update(e, &dev, c, log, live);
    frame_update(e, &dev, c, log, live);
    CHECK_FALSE(e.playing());
    CHECK_FALSE(live.playing);
    CHECK(count_level(log, LogLevel::Error) == 1);
    CHECK(log.lines().back().text == "Audio device lost");
}

TEST_CASE("meters decay to zero") {
    Engine e;
    Document d;
    Controller c(d, e);
    Log log;
    LiveState live;
    c.sync_engine();
    c.play();
    float out[2 * 2400];
    e.render(out, 2400);  // a metronome click: peak well above 0
    frame_update(e, nullptr, c, log, live);
    CHECK(live.meter_left > 0.1f);
    CHECK(needs_frames(live));
    c.stop();
    for (int i = 0; i < 200; ++i) frame_update(e, nullptr, c, log, live);
    CHECK(live.meter_left == 0.0f);
    CHECK_FALSE(needs_frames(live));
}
