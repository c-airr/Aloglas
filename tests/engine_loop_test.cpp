// Loop brace and FL-style play origin. 120 BPM, 4/4, 48 kHz:
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

TEST_CASE("loop jumps back on the exact sample and clicks the accent") {
    for (uint32_t block : {512u, 1u}) {
        CAPTURE(block);
        Engine e;
        setup(e);
        e.send(Command::set_loop(true, 3840, 7680));
        e.send(Command::play());
        render_frames(e, 200'000, block);

        const auto f = click_frames(e);
        const auto a = click_accents(e);
        // Play starts at the loop start (bar 2, an accent), the loop is one
        // bar long, so the 5th click is the jump back and is accented again.
        REQUIRE(f.size() == 9);
        for (size_t i = 0; i < f.size(); ++i) CHECK(std::llabs(f[i] - int64_t(i) * 24'000) <= 1);
        CHECK(a[0]);
        CHECK_FALSE(a[1]);
        CHECK(a[4]);
        CHECK(a[8]);
        CHECK(e.position_ticks() >= 3840.0);
        CHECK(e.position_ticks() < 7680.0);
    }
}

TEST_CASE("play origin: loop start when enabled, start marker otherwise") {
    Engine e;
    setup(e);
    e.send(Command::set_start(9600));
    e.send(Command::set_loop(false, 3840, 7680));
    e.send(Command::play());
    e.pump();
    CHECK(e.position_ticks() == 9600.0);

    e.send(Command::stop());
    e.send(Command::set_loop(true, 3840, 7680));
    e.send(Command::play());
    e.pump();
    CHECK(e.position_ticks() == 3840.0);
}

TEST_CASE("stop returns to the origin evaluated at stop time") {
    Engine e;
    setup(e);
    e.send(Command::play());
    render_frames(e, 50'000);
    e.send(Command::set_start(7680));
    render_frames(e, 1000);
    CHECK(e.position_ticks() > 2000.0);  // still playing from the old origin
    e.send(Command::stop());
    e.pump();
    CHECK(e.position_ticks() == 7680.0);
}

TEST_CASE("while stopped the playhead follows the origin") {
    Engine e;
    setup(e);
    e.send(Command::set_start(1920));
    e.pump();
    CHECK(e.position_ticks() == 1920.0);
    e.send(Command::set_loop(true, 3840, 7680));
    e.pump();
    CHECK(e.position_ticks() == 3840.0);
    e.send(Command::set_loop(false, 3840, 7680));
    e.pump();
    CHECK(e.position_ticks() == 1920.0);
}

TEST_CASE("playhead past the loop end does not loop") {
    Engine e;
    setup(e);
    e.send(Command::set_loop(true, 3840, 7680));
    e.send(Command::play());
    e.send(Command::seek(11520));
    render_frames(e, 48'000);
    CHECK(e.position_ticks() > 11520.0);
}
