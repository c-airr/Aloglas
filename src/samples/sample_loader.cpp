#include "samples/sample_loader.h"

#include <algorithm>

namespace aloglas {

SampleLoader::SampleLoader(DecodeFn decode) : decode_(std::move(decode)), worker_([this] { run(); }) {}

SampleLoader::~SampleLoader() {
    {
        std::lock_guard lock(mutex_);
        stop_ = true;
        requests_.clear();
    }
    wake_.notify_one();
    worker_.join();
}

uint64_t SampleLoader::request(const std::filesystem::path& path, bool preview) {
    uint64_t id;
    {
        std::lock_guard lock(mutex_);
        if (preview) std::erase_if(requests_, [](const Request& r) { return r.preview; });
        id = next_id_++;
        requests_.push_back({id, path, preview});
    }
    wake_.notify_one();
    return id;
}

std::vector<LoadResult> SampleLoader::take_results() {
    std::lock_guard lock(mutex_);
    return std::exchange(results_, {});
}

void SampleLoader::run() {
    for (;;) {
        Request req;
        {
            std::unique_lock lock(mutex_);
            wake_.wait(lock, [this] { return stop_ || !requests_.empty(); });
            if (stop_) return;
            req = std::move(requests_.front());
            requests_.pop_front();
        }
        // Decoding happens without the lock, so request() never waits for it.
        DecodeResult d = decode_(req.path);
        {
            std::lock_guard lock(mutex_);
            if (stop_) return;
            results_.push_back({req.id, std::move(req.path), req.preview, std::move(d.sample), std::move(d.error)});
        }
        if (on_result) on_result();
    }
}

}  // namespace aloglas
