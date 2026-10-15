// Decoded samples by path, so a file used by the preview and several sounds
// is decoded once. Holds weak pointers: a sample lives only while something
// (a preview, a sound) uses it. UI thread only.
#pragma once

#include <filesystem>
#include <map>
#include <memory>

#include "samples/sample.h"

namespace aloglas {

class SampleCache {
public:
    std::shared_ptr<const Sample> find(const std::filesystem::path& path) const;
    void put(std::shared_ptr<const Sample> sample);  // keyed by sample->path

private:
    std::map<std::filesystem::path, std::weak_ptr<const Sample>> entries_;
};

}  // namespace aloglas
