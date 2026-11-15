// compile_snapshot: what the audio thread is allowed to see of the document.
#include <doctest.h>

#include <cmath>
#include <memory>

#include "app/compile.h"
#include "app/document.h"
#include "samples/sample.h"

using namespace aloglas;

namespace {
std::shared_ptr<const Sample> dc(float value, int frames = 48000) {
    auto s = std::make_shared<Sample>();
    s->frames.assign(static_cast<std::size_t>(frames), value);
    s->sample_rate = 48000;
    return s;
}

Sound sound(SoundId id, std::shared_ptr<const Sample> sample = dc(0.5f)) {
    Sound s;
    s.id = id;
    s.name = "kick.wav";
    s.sample_path = "C:/samples/kick.wav";
    s.sample = std::move(sample);
    return s;
}
}  // namespace

TEST_CASE("sounds compile to gain, pan, mute and sampler settings") {
    Document d;
    Sound left = sound(1);
    left.volume_db = -6.0206f;  // half amplitude
    left.pan = -1;              // hard left
    Sound right = sound(2);
    right.pan = 1;  // hard right
    right.muted = true;
    right.root = 48;
    right.fine = 25;
    right.cut_itself = true;
    right.gate = true;
    right.glide_ms = 120;
    d.sounds = {left, right};

    const auto snapshot = compile_snapshot(d);
    REQUIRE(snapshot->sounds.size() == 2);
    const SnapshotSound& a = snapshot->sounds[0];
    CHECK(a.id == 1);
    CHECK(a.gain == doctest::Approx(0.5f).epsilon(0.001));
    CHECK(a.pan_left == doctest::Approx(1.0f));
    CHECK(a.pan_right == doctest::Approx(0.0f).epsilon(0.001));
    CHECK(a.audible);
    const SnapshotSound& b = snapshot->sounds[1];
    CHECK(b.pan_left == doctest::Approx(0.0f).epsilon(0.001));
    CHECK(b.pan_right == doctest::Approx(1.0f));
    CHECK_FALSE(b.audible);
    CHECK(b.root == 48);
    CHECK(b.fine == doctest::Approx(25.0f));
    CHECK(b.cut_itself);
    CHECK(b.gate);
    CHECK(b.glide_ms == doctest::Approx(120.0));
}

TEST_CASE("invalid and duplicate sounds are skipped") {
    Document d;
    Sound zero = sound(0);
    Sound duplicate = sound(2);
    Sound first = sound(2);
    d.sounds = {zero, first, duplicate};

    const auto snapshot = compile_snapshot(d);
    REQUIRE(snapshot->sounds.size() == 1);
    CHECK(snapshot->sounds[0].id == 2);
}

TEST_CASE("out-of-range settings are clamped, not trusted") {
    Document d;
    Sound s = sound(1);
    s.volume_db = 100;
    s.pan = 5;
    s.fine = -400;
    s.root = 300;
    s.glide_ms = 1e9;
    d.sounds = {s};

    const auto snapshot = compile_snapshot(d);
    REQUIRE(snapshot->sounds.size() == 1);
    CHECK(snapshot->sounds[0].gain == doctest::Approx(std::pow(10.0f, 6.0f / 20.0f)));
    CHECK(snapshot->sounds[0].pan_left == doctest::Approx(0.0f).epsilon(0.001));
    CHECK(snapshot->sounds[0].pan_right == doctest::Approx(1.0f));
    CHECK(snapshot->sounds[0].root == 127);
    CHECK(snapshot->sounds[0].fine == doctest::Approx(-100.0f));
    CHECK(snapshot->sounds[0].glide_ms == doctest::Approx(2000.0));
}

TEST_CASE("loop notes are sorted, trimmed to the loop and tied to sounds") {
    Document d;
    d.signature = {3, 4};  // the loop is one 3/4 bar: 2880 ticks
    d.sounds = {sound(1)};

    Note late;
    late.sound_id = 1;
    late.start_tick = 2000;
    late.length_ticks = 2000;  // past the loop end: trimmed
    Note early;
    early.sound_id = 1;
    early.start_tick = 100;
    early.length_ticks = 400;
    Note unknown = early;
    unknown.sound_id = 9;
    unknown.start_tick = 0;
    Note zero_length = early;
    zero_length.start_tick = 300;
    zero_length.length_ticks = 0;
    Note behind = early;
    behind.start_tick = -50;
    d.loop_notes = {late, early, unknown, zero_length, behind};

    const auto snapshot = compile_snapshot(d);
    CHECK(snapshot->loop_length_ticks == 2880);
    REQUIRE(snapshot->loop_events.size() == 2);  // negative starts are skipped, not moved
    CHECK(snapshot->loop_events[0].start_tick == 100);
    CHECK(snapshot->loop_events[0].end_tick == 500);
    CHECK(snapshot->loop_events[1].start_tick == 2000);
    CHECK(snapshot->loop_events[1].end_tick == 2880);  // trimmed
}

TEST_CASE("velocity is clamped and the note keeps its slide flag") {
    Document d;
    d.sounds = {sound(1)};
    Note note;
    note.sound_id = 1;
    note.pitch = 60;
    note.velocity = 7;
    note.slide = true;
    d.loop_notes = {note};

    const auto snapshot = compile_snapshot(d);
    REQUIRE(snapshot->loop_events.size() == 1);
    CHECK(snapshot->loop_events[0].velocity == doctest::Approx(1.0f));
    CHECK(snapshot->loop_events[0].slide);
}

TEST_CASE("an empty document compiles to an empty snapshot") {
    Document d;
    const auto snapshot = compile_snapshot(d);
    CHECK(snapshot);
    CHECK(snapshot->sounds.empty());
    CHECK(snapshot->loop_events.empty());
    CHECK(snapshot->loop_length_ticks == 3840);
}
