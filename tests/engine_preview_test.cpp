// Preview voice inside the engine: ownership of payloads and the garbage queue.
#include <doctest.h>

#include <memory>
#include <vector>

#include "core/rt_guard.h"
#include "engine/engine.h"
#include "engine/preview.h"
#include "support/render.h"

using namespace aloglas;
using namespace aloglas::test;

namespace {
// A DC sample owned only by the payload; the test keeps a weak pointer to
// see when the engine has given it back and the UI has freed it.
PreviewPayload* payload(float value, int frames, std::weak_ptr<const Sample>& watch) {
    auto s = std::make_shared<Sample>();
    s->frames.assign(static_cast<std::size_t>(frames), value);
    s->sample_rate = 48000;
    watch = s;
    return new PreviewPayload{std::move(s)};
}

Engine& quiet(Engine& e) {
    e.prepare(48000);
    e.send(Command::set_metronome(false));
    return e;
}
}  // namespace

TEST_CASE("preview plays at -6 dB and the payload comes back") {
    Engine e;
    quiet(e);
    std::weak_ptr<const Sample> s;
    e.send(Command::preview_sample(payload(1.0f, 480, s)));
    auto out = render_frames(e, 100);
    CHECK(out[0] == doctest::Approx(0.5012f));
    CHECK(out[1] == doctest::Approx(0.5012f));
    CHECK(e.previewing());
    render_frames(e, 1000);
    CHECK_FALSE(e.previewing());
    CHECK_FALSE(s.expired());  // returned, but only the UI thread frees it
    e.collect_garbage();
    CHECK(s.expired());
}

TEST_CASE("a new preview fades the previous one out") {
    Engine e;
    quiet(e);
    std::weak_ptr<const Sample> a, b;
    e.send(Command::preview_sample(payload(1.0f, 48000, a)));
    render_frames(e, 100);
    e.send(Command::preview_sample(payload(0.25f, 48000, b)));
    const auto out = render_frames(e, 300);
    CHECK(out[0] > 0.25f * 0.5012f);  // A still fading in
    CHECK(out[2 * 260] == doctest::Approx(0.25f * 0.5012f));
    e.collect_garbage();
    CHECK(a.expired());
    CHECK_FALSE(b.expired());
}

TEST_CASE("three previews in a row never hold more than two") {
    Engine e;
    quiet(e);
    std::weak_ptr<const Sample> a, b, c;
    e.send(Command::preview_sample(payload(1.0f, 48000, a)));
    e.send(Command::preview_sample(payload(1.0f, 48000, b)));
    e.send(Command::preview_sample(payload(1.0f, 48000, c)));
    render_frames(e, 1);
    e.collect_garbage();
    CHECK(a.expired());
    CHECK_FALSE(b.expired());
    CHECK_FALSE(c.expired());
    render_frames(e, 300);
    e.collect_garbage();
    CHECK(b.expired());
    CHECK_FALSE(c.expired());
}

TEST_CASE("stop_preview fades and returns") {
    Engine e;
    quiet(e);
    std::weak_ptr<const Sample> a;
    e.send(Command::preview_sample(payload(1.0f, 48000, a)));
    render_frames(e, 10);
    e.send(Command::stop_preview());
    render_frames(e, 300);
    CHECK_FALSE(e.previewing());
    e.collect_garbage();
    CHECK(a.expired());
}

TEST_CASE("dropped preview command frees its payload") {
    Engine e;
    e.set_assert_on_drop(false);
    while (e.send(Command::stop())) {
    }
    std::weak_ptr<const Sample> a;
    CHECK_FALSE(e.send(Command::preview_sample(payload(1.0f, 10, a))));
    CHECK(a.expired());
}

TEST_CASE("engine destructor frees held previews") {
    std::weak_ptr<const Sample> playing, queued;
    {
        Engine e;
        quiet(e);
        e.send(Command::preview_sample(payload(1.0f, 48000, playing)));
        render_frames(e, 10);
        e.send(Command::preview_sample(payload(1.0f, 48000, queued)));  // never applied
    }
    CHECK(playing.expired());
    CHECK(queued.expired());
}

TEST_CASE("render does not allocate or free with previews") {
    if (!rt::guard_enabled()) {
        MESSAGE("rt guard disabled in this build");
        return;
    }
    rt::set_abort_on_violation(false);
    Engine e;
    quiet(e);
    std::vector<float> buf(4096 * 2);
    std::weak_ptr<const Sample> a, b;
    PreviewPayload* pa = payload(1.0f, 2000, a);
    PreviewPayload* pb = payload(0.5f, 2000, b);
    const auto before = rt::violation_count();
    e.send(Command::preview_sample(pa));
    e.render(buf.data(), 512);
    e.send(Command::preview_sample(pb));
    e.render(buf.data(), 4096);  // both finish and go to the garbage queue
    e.send(Command::set_play_mode(PlayMode::Loop));
    e.send(Command::set_loop_length(960));
    e.send(Command::set_metronome(true));
    e.send(Command::play());
    for (int i = 0; i < 13; ++i) e.render(buf.data(), 4096);  // ~53'000 frames: many LOOP wraps
    CHECK(rt::violation_count() == before);
    rt::set_abort_on_violation(true);
    e.collect_garbage();
    CHECK(a.expired());
    CHECK(b.expired());
}
