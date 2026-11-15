// Snapshot playback in the engine: loop notes, preview notes, mute and the
// ownership hand-back when a snapshot is replaced.
#include <doctest.h>

#include <memory>
#include <vector>

#include "engine/engine.h"
#include "support/render.h"

using namespace aloglas;
using namespace aloglas::test;

namespace {
constexpr float kPan = 0.70710678f;

std::shared_ptr<const Sample> dc(float value, int frames = 48000) {
    auto s = std::make_shared<Sample>();
    s->frames.assign(static_cast<std::size_t>(frames), value);
    s->sample_rate = 48000;
    return s;
}

Snapshot* make_snapshot(std::shared_ptr<const Sample> sample, std::vector<NoteEvent> events = {},
                        bool audible = true) {
    auto s = std::make_unique<Snapshot>();
    s->loop_length_ticks = 3840;
    s->sounds.push_back({1, std::move(sample), 1.0f, kPan, kPan, audible, 60, 0, false, false, 80});
    s->loop_events = std::move(events);
    return s.release();
}

void setup(Engine& e) {
    e.prepare(48000);
    e.send(Command::set_tempo(120));
    e.send(Command::set_metronome(false));
    e.send(Command::set_play_mode(PlayMode::Loop));
    e.send(Command::set_loop_length(3840));
    e.send(Command::set_start(0));
    e.send(Command::set_loop(false, 0, 0));
}

NoteEvent note(int64_t start, int64_t end = 480, int pitch = 60, float velocity = 1.0f) {
    return {1, start, end, pitch, velocity, false};
}
}  // namespace

TEST_CASE("a loop note at tick zero sounds on the first frame") {
    Engine e;
    setup(e);
    e.send(Command::set_snapshot(make_snapshot(dc(0.5f), {note(0)})));
    e.send(Command::play());
    const auto out = render_frames(e, 4);
    CHECK(out[0] == doctest::Approx(0.5f * kPan));
    CHECK(out[1] == doctest::Approx(0.5f * kPan));
}

TEST_CASE("a note at tick 960 starts at frame 24000") {
    Engine e;
    setup(e);
    e.send(Command::set_snapshot(make_snapshot(dc(0.5f), {note(960)})));
    e.send(Command::play());
    const auto out = render_frames(e, 24'010);
    int64_t first = -1;
    for (int64_t i = 0; i < 24'010; ++i) {
        if (out[2 * i] != 0.0f) {
            first = i;
            break;
        }
    }
    REQUIRE(first >= 24'000);  // never early
    CHECK(first <= 24'001);    // one sample of rounding at most
    CHECK(out[2 * first] == doctest::Approx(0.5f * kPan));
}

TEST_CASE("a muted sound is triggered but silent") {
    Engine e;
    setup(e);
    e.send(Command::set_snapshot(make_snapshot(dc(0.5f), {note(0)}, false)));
    e.send(Command::play());
    const auto out = render_frames(e, 100);
    for (float x : out) CHECK(x == 0.0f);
}

TEST_CASE("a note loops with the pattern") {
    Engine e;
    setup(e);
    e.send(Command::set_snapshot(make_snapshot(dc(0.5f, 48'000), {note(0, 240)})));
    e.send(Command::play());
    // One bar is 96000 frames at 120 BPM; skip just past it and expect the
    // note again right after the wrap.
    render_skip(e, 96'000);
    const auto out = render_frames(e, 16);
    bool sounding = false;
    for (std::size_t i = 0; i < 16; ++i) sounding |= out[2 * i] != 0.0f;
    CHECK(sounding);
}

TEST_CASE("preview_note plays without the transport") {
    Engine e;
    setup(e);
    e.send(Command::set_snapshot(make_snapshot(dc(0.5f))));
    e.send(Command::preview_note(1, 60, 0.5f));
    const auto out = render_frames(e, 4);
    CHECK_FALSE(e.playing());
    CHECK(out[0] == doctest::Approx(0.25f * kPan));
}

TEST_CASE("replacing a snapshot gives the old one back to the UI") {
    Engine e;
    setup(e);
    std::weak_ptr<const Sample> watch;
    {
        auto sample = dc(0.5f);
        watch = sample;
        e.send(Command::set_snapshot(make_snapshot(sample)));
    }
    render_frames(e, 1);
    e.send(Command::set_snapshot(new Snapshot));
    render_frames(e, 1);
    CHECK_FALSE(watch.expired());  // the voice-free old snapshot waits in the queue
    e.collect_garbage();
    CHECK(watch.expired());
}

TEST_CASE("a removed sound fades its voice and then frees the sample") {
    Engine e;
    setup(e);
    std::weak_ptr<const Sample> watch;
    {
        auto sample = dc(0.5f, 480'000);
        watch = sample;
        e.send(Command::set_snapshot(make_snapshot(sample, {note(0)})));
    }
    e.send(Command::play());
    render_frames(e, 100);
    e.send(Command::set_snapshot(new Snapshot));  // sound gone while its note plays
    render_frames(e, 1000);                       // 5 ms fade
    e.collect_garbage();
    CHECK(watch.expired());
}
