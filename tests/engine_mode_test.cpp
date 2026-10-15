// LOOP/SONG play modes, loop length and the master peak. 120 BPM, 4/4, 48 kHz:
// one beat = 960 ticks = 24000 frames, one bar = 3840 ticks = 96000 frames.
#include <doctest.h>

#include <cstdlib>

#include "engine/engine.h"
#include "support/render.h"

using namespace aloglas;
using namespace aloglas::test;

namespace {
void setup(Engine& e) {
    e.prepare(48000);
    e.send(Command::set_tempo(120));
    e.send(Command::set_signature({4, 4}));
    e.send(Command::set_metronome(true));
}
}  // namespace

TEST_CASE("LOOP mode starts at 0 and wraps at the loop length") {
    Engine e;
    setup(e);
    e.send(Command::set_play_mode(PlayMode::Loop));
    e.send(Command::set_loop_length(3840));
    e.send(Command::set_start(7680));  // ignored in LOOP mode
    e.send(Command::play());
    render_frames(e, 96'000 + 10);

    CHECK(e.play_mode() == PlayMode::Loop);
    CHECK(e.position_ticks() < 3840.0);
    const auto f = click_frames(e);
    const auto a = click_accents(e);
    REQUIRE(f.size() == 5);
    for (size_t i = 0; i < f.size(); ++i) CHECK(std::llabs(f[i] - int64_t(i) * 24'000) <= 1);
    CHECK(a[0]);
    CHECK_FALSE(a[1]);
    CHECK(a[4]);
}

TEST_CASE("loop length shrinking under the playhead wraps") {
    Engine e;
    setup(e);
    e.send(Command::set_play_mode(PlayMode::Loop));
    e.send(Command::set_loop_length(3840));
    e.send(Command::play());
    render_frames(e, 75'000);  // ~3000 ticks: inside beat 4
    REQUIRE(e.position_ticks() > 2880.0);

    e.send(Command::set_loop_length(1920));
    render_frames(e, 1);
    CHECK(e.position_ticks() < 1920.0);
}

TEST_CASE("play mode change while playing jumps to the new origin") {
    Engine e;
    setup(e);
    e.send(Command::set_play_mode(PlayMode::Song));
    e.send(Command::set_start(7680));
    e.send(Command::play());
    render_frames(e, 1000);
    REQUIRE(e.position_ticks() > 7680.0);

    e.clear_click_log();
    const int64_t switched_at = e.frames_rendered();
    e.send(Command::set_play_mode(PlayMode::Loop));
    render_frames(e, 1);
    CHECK(e.playing());
    CHECK(e.position_ticks() < 100.0);
    REQUIRE(e.click_count() == 1);
    CHECK(e.click(0).frame == switched_at);
    CHECK(e.click(0).accent);
}

TEST_CASE("SONG mode ignores the loop length") {
    Engine e;
    setup(e);
    e.send(Command::set_loop_length(3840));
    e.send(Command::play());
    render_frames(e, 2 * 96'000);
    CHECK(e.play_mode() == PlayMode::Song);
    CHECK(e.position_ticks() > 3840.0);
}

TEST_CASE("master peak is taken and reset") {
    Engine e;
    setup(e);
    e.send(Command::play());
    render_frames(e, 2400);
    const StereoPeak p = e.take_master_peak();
    CHECK(p.left > 0.1f);
    CHECK(p.right > 0.1f);
    const StereoPeak again = e.take_master_peak();
    CHECK(again.left == 0.0f);
    CHECK(again.right == 0.0f);
}

TEST_CASE("invalid loop length is ignored") {
    Engine e;
    setup(e);
    e.send(Command::set_play_mode(PlayMode::Loop));
    e.send(Command::set_loop_length(3840));
    e.send(Command::set_loop_length(0));
    e.send(Command::set_loop_length(-5));
    e.send(Command::play());
    render_frames(e, 96'000 + 10);
    CHECK(e.position_ticks() < 3840.0);
    CHECK(e.position_ticks() > 0.0);
}
