// An audio file decoded into memory: what the browser previews and the
// sampler plays. Immutable once decoded, so the audio thread can
// read it while the UI thread holds it too.
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace aloglas {

struct Sample {
    std::vector<float> frames;  // interleaved: L R L R ... (or one value per frame for mono)
    int channels = 1;           // 1 or 2
    uint32_t sample_rate = 0;   // the file's own rate; playback converts it
    std::filesystem::path path;

    int64_t frame_count() const { return static_cast<int64_t>(frames.size()) / channels; }
};

// Exactly one of the two is set.
struct DecodeResult {
    std::shared_ptr<const Sample> sample;
    std::string error;
};

// Decodes WAV, AIFF, FLAC or MP3 to float at the file's own rate. More than
// two channels are downmixed to stereo. Files longer than `max_seconds` are
// rejected (a stray hour-long recording would take gigabytes as floats).
DecodeResult decode_sample(const std::filesystem::path& path, double max_seconds = 600.0);

}  // namespace aloglas
