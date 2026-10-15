// Decoding the committed fixtures in tests/data (0.1 s, 440 Hz, amplitude 1/8).
#include <doctest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>

#include "samples/sample.h"

using namespace aloglas;
namespace fs = std::filesystem;

namespace {
fs::path data(const char* name) {
    return fs::path(ALOGLAS_TEST_DATA) / name;
}

float peak(const Sample& s) {
    float p = 0;
    for (float f : s.frames) p = std::max(p, std::fabs(f));
    return p;
}
}  // namespace

TEST_CASE("decodes WAV, AIFF and FLAC exactly") {
    for (const char* name : {"sine_mono_44k.wav", "sine_mono_44k.aiff", "sine_mono_44k.flac"}) {
        CAPTURE(name);
        const DecodeResult r = decode_sample(data(name));
        REQUIRE(r.sample);
        CHECK(r.error.empty());
        CHECK(r.sample->channels == 1);
        CHECK(r.sample->sample_rate == 44100);
        CHECK(r.sample->frame_count() == 4410);
        CHECK(peak(*r.sample) == doctest::Approx(0.125).epsilon(0.02));
        CHECK(r.sample->path == data(name));
    }
}

TEST_CASE("decodes MP3") {
    const DecodeResult r = decode_sample(data("sine_mono_44k.mp3"));
    REQUIRE(r.sample);
    CHECK(r.sample->channels == 1);
    CHECK(r.sample->sample_rate == 44100);
    CHECK(r.sample->frame_count() >= 4410);
    CHECK(r.sample->frame_count() <= 4410 + 2400);  // encoder padding
}

TEST_CASE("keeps stereo") {
    const DecodeResult r = decode_sample(data("sine_stereo_48k.wav"));
    REQUIRE(r.sample);
    CHECK(r.sample->channels == 2);
    CHECK(r.sample->sample_rate == 48000);
    CHECK(r.sample->frames.size() == 2 * 4800);
}

TEST_CASE("downmixes more than two channels") {
    const DecodeResult r = decode_sample(data("sine_6ch_48k.wav"));
    REQUIRE(r.sample);
    CHECK(r.sample->channels == 2);
    CHECK(r.sample->frame_count() == 4800);
}

TEST_CASE("decodes a file with a non-ASCII path") {
    const fs::path dir = fs::temp_directory_path() / u8"Sample żółć";
    fs::create_directories(dir);
    const fs::path file = dir / u8"Kick ą.wav";
    fs::copy_file(data("sine_mono_44k.wav"), file, fs::copy_options::overwrite_existing);
    const DecodeResult r = decode_sample(file);
    REQUIRE(r.sample);
    CHECK(r.sample->frame_count() == 4410);
    fs::remove_all(dir);
}

TEST_CASE("rejects garbage, empty and too long files") {
    DecodeResult r = decode_sample(data("not_audio.wav"));
    CHECK_FALSE(r.sample);
    CHECK(r.error.find("cannot read") != std::string::npos);

    r = decode_sample(data("empty.wav"));
    CHECK_FALSE(r.sample);
    CHECK_FALSE(r.error.empty());

    r = decode_sample(data("does_not_exist.wav"));
    CHECK_FALSE(r.sample);
    CHECK(r.error.find("cannot read") != std::string::npos);

    r = decode_sample(data("sine_mono_44k.wav"), 0.05);
    CHECK_FALSE(r.sample);
    CHECK(r.error.find("longer than") != std::string::npos);
}
