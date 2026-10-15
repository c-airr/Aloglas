#include "samples/sample_cache.h"

namespace aloglas {

std::shared_ptr<const Sample> SampleCache::find(const std::filesystem::path& path) const {
    const auto it = entries_.find(path.lexically_normal());
    return it == entries_.end() ? nullptr : it->second.lock();
}

void SampleCache::put(std::shared_ptr<const Sample> sample) {
    std::erase_if(entries_, [](const auto& e) { return e.second.expired(); });
    entries_[sample->path.lexically_normal()] = sample;
}

}  // namespace aloglas
