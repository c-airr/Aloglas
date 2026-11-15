// The sampler instrument: pitch, gate, cut itself and generation fades.
#include <doctest.h>

#include <cmath>
#include <memory>
#include <vector>

#include "engine/sampler.h"
#include "samples/sample.h"

using namespace aloglas;

namespace {
std::shared_ptr<const Sample> dc(float value, int frames = 48000, uint32_t rate = 48000) {
    auto s = std::make_shared<Sample>();
    s->frames.assign(static_cast<std::size_t>(frames), value);
    s->sample_rate = rate;
    return s;
}

SnapshotSound sound(std::shared_ptr<const Sample> sample, int root = 60, bool cut_itself = false,
                    bool gate = false, double glide_ms = 80) {
    SnapshotSound s;
    s.id = 1;
    s.sample = std::move(sample);
    s.root = root;
    s.cut_itself = cut_itself;
    s.gate = gate;
    s.glide_ms = glide_ms;
    return s;
}

// Renders until no voice is left, with the playhead advancing at 120 BPM
// (0.04 ticks per sample at 48 kHz); returns the summed left channel.
std::vector<float> play(Sampler& sampler, int limit = 200'000) {
    std::vector<float> out;
    while (sampler.active() && static_cast<int>(out.size()) < limit) {
        const double tick = static_cast<double>(out.size()) * 0.04;
        float l = 0, r = 0;
        sampler.render_frame(tick, 48000, l, r);
        out.push_back(l);
    }
    return out;
}

void skip(Sampler& sampler, int frames, double from_tick = 0) {
    for (int i = 0; i < frames; ++i) {
        float l = 0, r = 0;
        sampler.render_frame(from_tick + i * 0.04, 48000, l, r);
    }
}
}  // namespace

TEST_CASE("a note plays at its velocity on both channels and ends with the sample") {
    const auto s = dc(0.25f, 100);
    Sampler sampler;
    sampler.note_on(sound(s), 1, 60, 0.5f, -1, false, 48000);
    CHECK(sampler.active());
    const auto out = play(sampler);
    REQUIRE(out.size() == 100);
    for (float x : out) CHECK(x == doctest::Approx(0.125f));
    CHECK_FALSE(sampler.active());
}

TEST_CASE("an octave up reads the sample twice as fast") {
    const auto s = dc(0.25f, 1000);
    Sampler sampler;
    sampler.note_on(sound(s), 1, 72, 1.0f, -1, false, 48000);  // root 60
    const auto out = play(sampler);
    CHECK(out.size() >= 498);
    CHECK(out.size() <= 502);
}

TEST_CASE("gate releases the note at its end tick") {
    const auto s = dc(1.0f);
    Sampler sampler;
    // 240 ticks at 120 BPM is 6000 frames (125 ms); the release fade is 5 ms.
    sampler.note_on(sound(s, 60, false, true), 1, 60, 1.0f, 240, false, 48000);
    const auto out = play(sampler);
    REQUIRE(out.size() > 6000);
    for (std::size_t i = 0; i < 6000; ++i) CHECK(out[i] == doctest::Approx(1.0f));
    CHECK(out.size() <= 6250);
    CHECK(out.back() < 0.1f);
}

TEST_CASE("cut itself fades the previous note instead of stopping it dead") {
    const auto s = dc(0.5f);
    Sampler sampler;
    const SnapshotSound cutting = sound(s, 60, true);
    sampler.note_on(cutting, 1, 60, 1.0f, -1, false, 48000);
    skip(sampler, 100);
    sampler.note_on(cutting, 1, 60, 0.5f, -1, false, 48000);
    float l = 0, r = 0;
    sampler.render_frame(4.0, 48000, l, r);
    CHECK(l > 0.6f);  // the old 0.5 is still fading out under the new 0.25
    for (int i = 0; i < 300; ++i) {
        l = r = 0;
        sampler.render_frame(4.0 + i * 0.04, 48000, l, r);
    }
    CHECK(l == doctest::Approx(0.25f));
}

TEST_CASE("only voices of the faded generation are silenced") {
    const auto s = dc(0.5f);
    Sampler sampler;
    sampler.note_on(sound(s), 1, 60, 1.0f, -1, false, 48000);
    sampler.note_on(sound(s), 2, 60, 1.0f, -1, false, 48000);
    float l = 0, r = 0;
    sampler.render_frame(0, 48000, l, r);
    CHECK(l == doctest::Approx(1.0f));  // two voices at half each

    sampler.fade_generation(1, 48000);
    float l2 = 0;
    for (int i = 0; i < 120; ++i) {
        l = r = 0;
        sampler.render_frame(0.04 + i * 0.04, 48000, l, r);
    }
    l2 = l;
    CHECK(l2 < 0.9f);  // generation 1 is fading; generation 2 is not
    for (int i = 0; i < 300; ++i) {
        l = r = 0;
        sampler.render_frame(5.0 + i * 0.04, 48000, l, r);
    }
    CHECK(l == doctest::Approx(0.5f));
    CHECK(sampler.generation_active(2));
    CHECK_FALSE(sampler.generation_active(1));
}
