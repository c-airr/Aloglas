#include <doctest.h>

#include <chrono>
#include <filesystem>
#include <thread>

#include "app/log.h"
#include "app/previewer.h"
#include "engine/preview.h"
#include "samples/sample_cache.h"
#include "samples/sample_loader.h"
#include "support/recording_sink.h"

using namespace aloglas;
using namespace aloglas::test;
using namespace std::chrono_literals;
namespace fs = std::filesystem;

namespace {
fs::path data(const char* name) {
    return fs::path(ALOGLAS_TEST_DATA) / name;
}

struct Fixture {
    RecordingSink sink;
    SampleLoader loader;
    SampleCache cache;
    Log log;
    Previewer previewer{sink, loader, cache, log};

    // Feeds loader results to the previewer until `n` arrived (or 5 s).
    void deliver(std::size_t n) {
        std::size_t got = 0;
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        while (got < n && std::chrono::steady_clock::now() < deadline) {
            for (const LoadResult& r : loader.take_results()) {
                previewer.on_result(r);
                ++got;
            }
            std::this_thread::sleep_for(5ms);
        }
    }

    int previews() const {
        int n = 0;
        for (const Command& c : sink.sent) n += c.type == CommandType::PreviewSample;
        return n;
    }
};
}  // namespace

TEST_CASE("preview loads then plays") {
    Fixture f;
    f.previewer.preview(data("sine_mono_44k.wav"));
    CHECK(f.sink.sent.empty());
    f.deliver(1);
    REQUIRE(f.previews() == 1);
    const Command& c = f.sink.sent.back();
    REQUIRE(c.preview);
    CHECK(c.preview->sample->frame_count() == 4410);
    CHECK(f.cache.find(data("sine_mono_44k.wav")) == c.preview->sample);
}

TEST_CASE("a cached sample plays at once") {
    Fixture f;
    auto s = decode_sample(data("sine_mono_44k.wav")).sample;
    f.cache.put(s);
    f.previewer.preview(data("sine_mono_44k.wav"));
    REQUIRE(f.previews() == 1);
    CHECK(f.sink.sent.back().preview->sample == s);
    std::this_thread::sleep_for(50ms);
    CHECK(f.loader.take_results().empty());  // nothing was requested
}

TEST_CASE("only the latest preview plays") {
    Fixture f;
    f.previewer.preview(data("sine_mono_44k.wav"));
    f.previewer.preview(data("sine_stereo_48k.wav"));
    // The first may or may not get decoded (it may be superseded first);
    // wait for the second one's result.
    bool second_done = false;
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (!second_done && std::chrono::steady_clock::now() < deadline) {
        for (const LoadResult& r : f.loader.take_results()) {
            f.previewer.on_result(r);
            second_done |= r.path == data("sine_stereo_48k.wav");
        }
        std::this_thread::sleep_for(5ms);
    }
    REQUIRE(second_done);
    REQUIRE(f.previews() == 1);
    CHECK(f.sink.sent.back().preview->sample->channels == 2);
}

TEST_CASE("no audio, no preview") {
    Fixture f;
    f.previewer.audio_ready = [] { return false; };
    f.previewer.preview(data("sine_mono_44k.wav"));
    std::this_thread::sleep_for(50ms);
    CHECK(f.loader.take_results().empty());
    CHECK(f.sink.sent.empty());
}

TEST_CASE("failed loads are logged") {
    Fixture f;
    f.previewer.preview(data("not_audio.wav"));
    f.deliver(1);
    CHECK(f.previews() == 0);
    REQUIRE(f.log.lines().size() == 1);
    CHECK(f.log.lines()[0].level == LogLevel::Error);
    CHECK(f.log.lines()[0].text.find("not_audio.wav") != std::string::npos);
}

TEST_CASE("stop forgets the wanted preview") {
    Fixture f;
    f.previewer.preview(data("sine_mono_44k.wav"));
    f.previewer.stop();
    f.deliver(1);
    CHECK(f.previews() == 0);
    REQUIRE_FALSE(f.sink.sent.empty());
    CHECK(f.sink.sent.back().type == CommandType::StopPreview);
}
