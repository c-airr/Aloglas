#include <doctest.h>

#include <cmath>
#include <cstdlib>

#include "core/rt_guard.h"
#include "engine/engine.h"
#include "support/render.h"

using namespace aloglas;
using namespace aloglas::test;

namespace {
Engine& start(Engine& e, double bpm, TimeSignature sig = {4, 4}) {
    e.prepare(48000);
    e.send(Command::set_tempo(bpm));
    e.send(Command::set_signature(sig));
    e.send(Command::set_metronome(true));
    e.send(Command::play());
    return e;
}
}  // namespace

TEST_CASE("clicks land on exact samples at 120 BPM") {
    Engine e;
    start(e, 120);
    render_frames(e, 100'000);
    const auto f = click_frames(e);
    REQUIRE(f.size() == 5);  // 0, 24000, 48000, 72000, 96000
    for (size_t i = 0; i < f.size(); ++i) CHECK(std::llabs(f[i] - int64_t(i) * 24000) <= 1);
}

TEST_CASE("no drift after 1000 beats at 133 BPM") {
    Engine e;
    start(e, 133);
    const double samples_per_beat = 60.0 * 48000 / 133;
    render_skip(e, static_cast<int64_t>(999 * samples_per_beat) + 100);
    const auto f = click_frames(e);
    REQUIRE(f.size() == 1000);
    CHECK(std::abs(f[999] - 999 * samples_per_beat) <= 1.0);
}

TEST_CASE("block size does not change the output") {
    std::vector<std::vector<float>> outs;
    std::vector<std::vector<int64_t>> clicks;
    for (uint32_t block : {64u, 480u, 1024u}) {
        Engine e;
        start(e, 140);
        outs.push_back(render_frames(e, 480'000, block));
        clicks.push_back(click_frames(e));
    }
    CHECK(clicks[0] == clicks[1]);
    CHECK(clicks[0] == clicks[2]);
    CHECK(outs[0] == outs[1]);
    CHECK(outs[0] == outs[2]);
}

TEST_CASE("tempo change mid-play") {
    Engine e;
    start(e, 120);
    render_frames(e, 48'000);  // clicks at 0 and 24000
    e.send(Command::set_tempo(240));
    render_frames(e, 48'000);
    const auto f = click_frames(e);
    REQUIRE(f.size() >= 4);
    CHECK(std::llabs(f[2] - 48'000) <= 1);
    CHECK(std::llabs((f[3] - f[2]) - 12'000) <= 1);
}

TEST_CASE("signatures: beat spacing and accents") {
    SUBCASE("6/8") {
        Engine e;
        start(e, 120, {6, 8});
        render_frames(e, 12'000 * 7 - 100);  // 7 clicks
        const auto f = click_frames(e);
        const auto a = click_accents(e);
        REQUIRE(f.size() == 7);
        CHECK(std::llabs((f[1] - f[0]) - 12'000) <= 1);
        for (size_t i = 0; i < 7; ++i) CHECK(a[i] == (i == 0 || i == 6));
    }
    SUBCASE("7/4") {
        Engine e;
        start(e, 120, {7, 4});
        render_frames(e, 24'000 * 8 - 100);  // 8 clicks
        const auto a = click_accents(e);
        REQUIRE(a.size() == 8);
        for (size_t i = 0; i < 8; ++i) CHECK(a[i] == (i == 0 || i == 7));
    }
    SUBCASE("4/4") {
        Engine e;
        start(e, 120, {4, 4});
        render_frames(e, 24'000 * 9 - 100);  // 9 clicks
        const auto a = click_accents(e);
        REQUIRE(a.size() == 9);
        for (size_t i = 0; i < 9; ++i) CHECK(a[i] == (i % 4 == 0));
    }
}

TEST_CASE("click sound: stereo, short, starts on its frame") {
    Engine e;
    start(e, 120);
    const auto out = render_frames(e, 30'000);
    bool stereo_equal = true;
    for (size_t i = 0; i < 30'000; ++i) stereo_equal &= out[2 * i] == out[2 * i + 1];
    CHECK(stereo_equal);

    float peak_start = 0.0f;
    for (size_t i = 0; i < 240; ++i) peak_start = std::max(peak_start, std::abs(out[2 * i]));
    CHECK(peak_start > 0.1f);

    bool silent_between = true;
    for (size_t i = 2400; i < 24'000; ++i) silent_between &= out[2 * i] == 0.0f;
    CHECK(silent_between);
}

TEST_CASE("stop silences and resets") {
    Engine e;
    start(e, 120);
    render_frames(e, 10'000);
    CHECK(e.playing());
    e.send(Command::stop());
    const auto out = render_frames(e, 1000);
    bool silent = true;
    for (float s : out) silent &= s == 0.0f;
    CHECK(silent);
    CHECK_FALSE(e.playing());
    CHECK(e.position_ticks() == 0.0);
}

TEST_CASE("metronome off renders silence but the playhead moves") {
    Engine e;
    start(e, 120);
    e.send(Command::set_metronome(false));
    const auto out = render_frames(e, 48'000);
    bool silent = true;
    for (float s : out) silent &= s == 0.0f;
    CHECK(silent);
    CHECK(e.click_count() == 0);
    CHECK(e.position_ticks() == doctest::Approx(1920.0).epsilon(0.001));
}

TEST_CASE("render does not allocate") {
    if (!rt::guard_enabled()) {
        MESSAGE("rt guard disabled in this build");
        return;
    }
    rt::set_abort_on_violation(false);
    Engine e;
    e.prepare(48000);
    std::vector<float> buf(4096 * 2);
    const auto before = rt::violation_count();
    e.send(Command::set_tempo(133));
    e.send(Command::set_signature({7, 8}));
    e.send(Command::set_metronome(true));
    e.send(Command::set_loop(true, 0, 3840));
    e.send(Command::set_start(960));
    e.send(Command::play());
    e.send(Command::seek(1920));
    e.render(buf.data(), 4096);
    e.send(Command::stop());
    e.render(buf.data(), 4096);
    e.pump();
    CHECK(rt::violation_count() == before);
    rt::set_abort_on_violation(true);
}

TEST_CASE("full queue drops and counts") {
    Engine e;
    e.set_assert_on_drop(false);
    int accepted = 0;
    for (int i = 0; i < 300; ++i) accepted += e.send(Command::set_tempo(100 + i)) ? 1 : 0;
    CHECK(accepted == 256);
    CHECK(e.dropped_commands() == 44);
}
