#include <doctest.h>

#include "app/controller.h"
#include "support/recording_sink.h"

using namespace aloglas;
using namespace aloglas::test;

TEST_CASE("defaults and sync_engine") {
    Document d;
    RecordingSink s;
    Controller c(d, s);
    c.sync_engine();
    REQUIRE(s.sent.size() == 7);
    CHECK(s.sent[0].type == CommandType::SetTempo);
    CHECK(s.sent[0].number == 140.0);
    CHECK(s.sent[1].type == CommandType::SetSignature);
    CHECK(s.sent[2].type == CommandType::SetMetronome);
    CHECK(s.sent[3].type == CommandType::SetLoop);
    CHECK_FALSE(s.sent[3].flag);
    CHECK(s.sent[3].b == 15360);
    CHECK(s.sent[4].type == CommandType::SetStart);
    CHECK(s.sent[5].type == CommandType::SetPlayMode);
    CHECK(s.sent[5].flag == false);  // LOOP
    CHECK(s.sent[6].type == CommandType::SetLoopLength);
    CHECK(s.sent[6].a == 3840);
}

TEST_CASE("tempo clamps and rounds") {
    Document d;
    RecordingSink s;
    Controller c(d, s);
    c.set_tempo(10);
    CHECK(d.tempo == 20.0);
    c.set_tempo(1500);
    CHECK(d.tempo == 999.0);
    c.set_tempo(140.456);
    CHECK(d.tempo == doctest::Approx(140.46));
    CHECK(s.sent.back().type == CommandType::SetTempo);
    CHECK(s.sent.back().number == doctest::Approx(140.46));
}

TEST_CASE("signature change revalidates brace, start marker and loop length") {
    Document d;
    RecordingSink s;
    Controller c(d, s);

    c.set_brace_range(0, 960);
    c.set_signature({4, 2});  // a beat is now a half note: 1920 ticks
    CHECK(d.brace.end == 1920);

    c.set_signature({4, 4});
    c.set_start_marker(999 * 3840, false);
    c.set_signature({1, 16});
    CHECK(d.start_marker == max_ticks({1, 16}) - 1);

    c.set_signature({3, 4});
    CHECK(s.sent.back().type == CommandType::SetLoopLength);
    CHECK(s.sent.back().a == 2880);

    int changes = 0;
    c.on_change = [&] { ++changes; };
    const auto before = s.sent.size();
    c.set_signature({4, 3});
    CHECK(s.sent.size() == before);
    CHECK(changes == 0);
}

TEST_CASE("brace normalization") {
    Document d;
    RecordingSink s;
    Controller c(d, s);
    c.set_brace_range(7680, 3840);  // reversed
    CHECK(d.brace.start == 3840);
    CHECK(d.brace.end == 7680);
    c.set_brace_range(100, 200);  // shorter than a beat
    CHECK(d.brace.start == 100);
    CHECK(d.brace.end == 1060);
    const int64_t max = max_ticks({4, 4});
    c.set_brace_range(max - 10, max + 500);  // too short after clamping, at the end
    CHECK(d.brace.end == max);
    CHECK(d.brace.start == max - 960);
    c.set_brace_range(-100, 960);
    CHECK(d.brace.start == 0);
    CHECK(s.sent.back().type == CommandType::SetLoop);
}

TEST_CASE("start marker seek") {
    Document d;
    RecordingSink s;
    Controller c(d, s);
    c.set_start_marker(1920, true);
    REQUIRE(s.sent.size() == 2);
    CHECK(s.sent[0].type == CommandType::SetStart);
    CHECK(s.sent[1].type == CommandType::Seek);
    CHECK(s.sent[1].a == 1920);
    s.sent.clear();
    c.set_start_marker(3840, false);
    REQUIRE(s.sent.size() == 1);
    CHECK(s.sent[0].type == CommandType::SetStart);
    c.set_start_marker(-5, false);
    CHECK(d.start_marker == 0);
}

TEST_CASE("play without audio sends nothing") {
    Document d;
    RecordingSink s;
    Controller c(d, s);
    bool audio = false;
    c.ensure_audio = [&] { return audio; };
    c.play();
    CHECK(s.sent.empty());
    audio = true;
    c.play();
    REQUIRE(s.sent.size() == 1);
    CHECK(s.sent[0].type == CommandType::Play);
    c.toggle_play(true);
    CHECK(s.sent.back().type == CommandType::Stop);
    c.toggle_play(false);
    CHECK(s.sent.back().type == CommandType::Play);
}

TEST_CASE("play mode") {
    Document d;
    RecordingSink s;
    Controller c(d, s);
    c.set_play_mode(PlayMode::Song);
    CHECK(d.play_mode == PlayMode::Song);
    REQUIRE(s.sent.size() == 1);
    CHECK(s.sent[0].type == CommandType::SetPlayMode);
    CHECK(s.sent[0].flag);
    c.set_play_mode(PlayMode::Song);
    CHECK(s.sent.size() == 1);
}

TEST_CASE("on_change fires only for real changes") {
    Document d;
    RecordingSink s;
    Controller c(d, s);
    int changes = 0;
    c.on_change = [&] { ++changes; };
    c.set_tempo(140);
    c.set_metronome(true);
    c.set_follow(true);
    c.set_brace_enabled(false);
    CHECK(changes == 0);
    CHECK(s.sent.empty());
    c.set_tempo(150);
    c.set_metronome(false);
    c.set_follow(false);
    c.toggle_brace();
    c.set_play_mode(PlayMode::Song);
    c.set_start_marker(960, false);
    CHECK(changes == 6);
    CHECK(d.brace.enabled);
}
