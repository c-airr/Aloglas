// Plays a browser file as a preview: from the cache when it is already
// decoded, otherwise after the loader has decoded it. Only the file the user
// wants *now* plays; results for files they have moved past are ignored.
#pragma once

#include <filesystem>
#include <functional>
#include <optional>

#include "app/log.h"
#include "engine/commands.h"
#include "samples/sample_cache.h"
#include "samples/sample_loader.h"

namespace aloglas {

class Previewer {
public:
    Previewer(CommandSink& engine, SampleLoader& loader, SampleCache& cache, Log& log)
        : engine_(engine), loader_(loader), cache_(cache), log_(log) {}

    // False = no audio device running: then nothing is decoded or sent.
    std::function<bool()> audio_ready;

    void preview(const std::filesystem::path& path);
    void stop();
    // UI thread, for every loader result (also non-preview ones: they fill the cache).
    void on_result(const LoadResult& result);

private:
    void play(std::shared_ptr<const Sample> sample);

    CommandSink& engine_;
    SampleLoader& loader_;
    SampleCache& cache_;
    Log& log_;
    std::optional<std::filesystem::path> wanted_;
};

}  // namespace aloglas
