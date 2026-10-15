#include <doctest.h>

#include <vector>

#include "engine/sample_voice.h"

using namespace aloglas;

namespace {
Sample dc(float value, int frames, uint32_t rate = 48000, int channels = 1) {
    Sample s;
    s.frames.assign(static_cast<std::size_t>(frames * channels), value);
    s.channels = channels;
    s.sample_rate = rate;
    return s;
}

// Renders until the voice stops (or `limit` frames); returns the left channel.
std::vector<float> play(SampleVoice& v, int limit = 100'000) {
    std::vector<float> out;
    while (v.active() && static_cast<int>(out.size()) < limit) {
        float l = 0, r = 0;
        v.render_frame(l, r);
        out.push_back(l);
    }
    return out;
}
}  // namespace

TEST_CASE("voice plays a sample at the same rate frame by frame") {
    const Sample s = dc(0.5f, 100);
    SampleVoice v;
    v.start(s, 1.0, 1.0f);
    const auto out = play(v);
    REQUIRE(out.size() == 100);
    for (float x : out) CHECK(x == 0.5f);
    CHECK_FALSE(v.active());
}

TEST_CASE("voice converts the sample rate") {
    const Sample s = dc(0.5f, 100, 24000);
    SampleVoice v;
    v.start(s, 0.5, 1.0f);  // a 24 kHz file on a 48 kHz device
    const auto out = play(v);
    CHECK(out.size() >= 199);
    CHECK(out.size() <= 200);
    for (std::size_t i = 4; i <= 190; ++i) CHECK(out[i] == doctest::Approx(0.5f));
}

TEST_CASE("Hermite interpolation passes through the samples and is smooth") {
    Sample s;
    s.sample_rate = 48000;
    for (int i = 0; i < 10; ++i) s.frames.push_back(static_cast<float>(i));
    SampleVoice v;
    v.start(s, 0.25, 1.0f);
    const auto out = play(v);
    REQUIRE(out.size() >= 37);
    for (int i = 0; i < 10; ++i) CHECK(out[static_cast<std::size_t>(i * 4)] == doctest::Approx(i));
    for (std::size_t i = 4; i < 28; ++i) CHECK(out[i + 1] > out[i]);  // positions 1..7: rising
}

TEST_CASE("mono feeds both channels, stereo stays apart") {
    const Sample mono = dc(0.25f, 10);
    SampleVoice v;
    v.start(mono, 1.0, 1.0f);
    float l = 0, r = 0;
    v.render_frame(l, r);
    CHECK(l == 0.25f);
    CHECK(r == 0.25f);

    Sample stereo;
    stereo.channels = 2;
    stereo.sample_rate = 48000;
    stereo.frames = {0.1f, -0.2f, 0.1f, -0.2f, 0.1f, -0.2f};
    v.start(stereo, 1.0, 2.0f);
    l = r = 0;
    v.render_frame(l, r);
    CHECK(l == doctest::Approx(0.2f));
    CHECK(r == doctest::Approx(-0.4f));
}

TEST_CASE("fade_out reaches silence within 5 ms") {
    const Sample s = dc(1.0f, 48000);
    SampleVoice v;
    v.start(s, 1.0, 1.0f);
    v.fade_out(48000);
    const auto out = play(v);
    CHECK(out.size() <= 240);
    CHECK(out.size() >= 230);
    for (std::size_t i = 1; i < out.size(); ++i) CHECK(out[i] < out[i - 1]);
}
