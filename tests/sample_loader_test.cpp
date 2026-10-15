#include <doctest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <future>
#include <thread>
#include <vector>

#include "samples/sample_cache.h"
#include "samples/sample_loader.h"

using namespace aloglas;
using namespace std::chrono_literals;
namespace fs = std::filesystem;

namespace {
fs::path data(const char* name) {
    return fs::path(ALOGLAS_TEST_DATA) / name;
}

// Polls the loader until `n` results arrived (or 5 s passed).
std::vector<LoadResult> wait_for(SampleLoader& loader, std::size_t n) {
    std::vector<LoadResult> all;
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (all.size() < n && std::chrono::steady_clock::now() < deadline) {
        for (LoadResult& r : loader.take_results()) all.push_back(std::move(r));
        std::this_thread::sleep_for(5ms);
    }
    return all;
}
}  // namespace

TEST_CASE("loader decodes in the background and reports errors") {
    SampleLoader loader;
    std::atomic<int> wakes{0};
    loader.on_result = [&] { ++wakes; };
    const uint64_t a = loader.request(data("sine_mono_44k.wav"), false);
    const uint64_t b = loader.request(data("not_audio.wav"), false);
    const auto results = wait_for(loader, 2);
    REQUIRE(results.size() == 2);
    CHECK(results[0].id == a);
    REQUIRE(results[0].sample);
    CHECK(results[0].sample->frame_count() == 4410);
    CHECK(results[1].id == b);
    CHECK_FALSE(results[1].sample);
    CHECK_FALSE(results[1].error.empty());
    CHECK(wakes == 2);
}

TEST_CASE("a new preview request replaces pending previews") {
    std::promise<void> release;
    std::shared_future<void> gate = release.get_future().share();
    std::atomic<int> decodes{0};
    SampleLoader loader([&](const fs::path& p) {
        if (decodes++ == 0) gate.wait();  // hold the worker inside the first decode
        return decode_sample(p);
    });
    loader.request(data("sine_mono_44k.wav"), true);
    while (decodes == 0) std::this_thread::sleep_for(1ms);  // the worker is now stuck in decode #1

    uint64_t last = 0;
    for (int i = 0; i < 29; ++i) last = loader.request(data("sine_stereo_48k.wav"), true);
    release.set_value();

    const auto results = wait_for(loader, 2);
    std::this_thread::sleep_for(50ms);
    CHECK(loader.take_results().empty());
    REQUIRE(results.size() == 2);
    CHECK(results[1].id == last);
    CHECK(results[1].preview);
    CHECK(decodes == 2);
}

TEST_CASE("non-preview requests are never dropped") {
    std::promise<void> release;
    std::shared_future<void> gate = release.get_future().share();
    std::atomic<int> decodes{0};
    SampleLoader loader([&](const fs::path& p) {
        if (decodes++ == 0) gate.wait();
        return decode_sample(p);
    });
    loader.request(data("sine_mono_44k.wav"), true);
    while (decodes == 0) std::this_thread::sleep_for(1ms);
    for (int i = 0; i < 5; ++i) {
        loader.request(data("sine_mono_44k.wav"), false);
        loader.request(data("sine_stereo_48k.wav"), true);
    }
    release.set_value();
    const auto results = wait_for(loader, 7);
    int normal = 0;
    for (const LoadResult& r : results) normal += !r.preview;
    CHECK(normal == 5);
    CHECK(results.size() == 7);  // first preview + 5 normal + the last preview
}

TEST_CASE("cache shares while used and forgets after") {
    SampleCache cache;
    auto s = std::make_shared<Sample>();
    s->path = fs::path("dir") / "x.wav";
    s->frames = {0.0f};
    cache.put(s);
    CHECK(cache.find(fs::path("dir") / "x.wav") == s);
    CHECK(cache.find(fs::path("dir") / "." / "x.wav") == s);
    CHECK(cache.find("other.wav") == nullptr);
    s.reset();
    CHECK(cache.find(fs::path("dir") / "x.wav") == nullptr);
}

TEST_CASE("destroying the loader with pending work joins cleanly") {
    SampleLoader loader;
    for (int i = 0; i < 20; ++i) loader.request(data("sine_stereo_48k.wav"), false);
    // The destructor runs here: it must stop and join without decoding all 20.
}
