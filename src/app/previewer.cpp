#include "app/previewer.h"

#include "engine/preview.h"

namespace aloglas {

void Previewer::play(std::shared_ptr<const Sample> sample) {
    // Ownership of the payload moves to the engine; it comes back through
    // the garbage queue (or send() frees it when the queue is full).
    engine_.send(Command::preview_sample(new PreviewPayload{std::move(sample)}));
}

void Previewer::preview(const std::filesystem::path& path) {
    wanted_ = path;
    if (audio_ready && !audio_ready()) return;
    if (auto cached = cache_.find(path)) {
        play(std::move(cached));
        return;
    }
    loader_.request(path, true);
}

void Previewer::stop() {
    wanted_.reset();
    engine_.send(Command::stop_preview());
}

void Previewer::on_result(const LoadResult& r) {
    if (!r.sample) {
        const std::u8string name = r.path.filename().u8string();
        log_.add(LogLevel::Error, "Cannot load " + std::string(name.begin(), name.end()) + ": " + r.error);
        return;
    }
    cache_.put(r.sample);
    if (r.preview && wanted_ && *wanted_ == r.path) play(r.sample);
}

}  // namespace aloglas
