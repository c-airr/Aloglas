// Decodes files on a background thread, so the window never waits for a
// 50 MB WAV. The UI asks with request() and collects with take_results()
// once per frame.
#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#include "samples/sample.h"

namespace aloglas {

struct LoadResult {
    uint64_t id = 0;
    std::filesystem::path path;
    bool preview = false;
    std::shared_ptr<const Sample> sample;  // or `error`
    std::string error;
};

class SampleLoader {
public:
    using DecodeFn = std::function<DecodeResult(const std::filesystem::path&)>;

    // `decode` is replaceable so tests can hold the worker inside a decode.
    explicit SampleLoader(DecodeFn decode = [](const std::filesystem::path& p) { return decode_sample(p); });
    ~SampleLoader();  // drops pending requests, waits for the current decode
    SampleLoader(const SampleLoader&) = delete;
    SampleLoader& operator=(const SampleLoader&) = delete;

    // A preview request replaces every preview still waiting: when the user
    // arrows through 50 files, only the one they stop on gets decoded.
    uint64_t request(const std::filesystem::path& path, bool preview);
    std::vector<LoadResult> take_results();

    // Called on the WORKER thread after each result is stored (to wake the
    // UI's event loop). Set it before the first request.
    std::function<void()> on_result;

private:
    struct Request {
        uint64_t id;
        std::filesystem::path path;
        bool preview;
    };

    void run();

    DecodeFn decode_;
    // The loader is not real-time, so a plain mutex is fine here (unlike the
    // engine, which must never block).
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Request> requests_;
    std::vector<LoadResult> results_;
    uint64_t next_id_ = 1;
    bool stop_ = false;
    std::thread worker_;  // last: starts after everything above exists
};

}  // namespace aloglas
