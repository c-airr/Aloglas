#include "samples/sample.h"

#include <miniaudio.h>

#include <cmath>

namespace aloglas {

namespace {

std::string name_of(const std::filesystem::path& p) {
    const std::u8string n = p.filename().u8string();
    return {n.begin(), n.end()};
}

// channels 0 = keep the file's channel count.
bool open(const std::filesystem::path& path, ma_uint32 channels, ma_decoder& dec) {
    const ma_decoder_config config = ma_decoder_config_init(ma_format_f32, channels, 0);
    // The _w variant takes a wide (UTF-16) path, so folders with Polish
    // letters open on Windows; the char* variant would use the ANSI code page.
    return ma_decoder_init_file_w(path.c_str(), &config, &dec) == MA_SUCCESS;
}

}  // namespace

DecodeResult decode_sample(const std::filesystem::path& path, double max_seconds) {
    const std::string name = name_of(path);
    ma_decoder dec;
    if (!open(path, 0, dec)) return {nullptr, "cannot read " + name};
    if (dec.outputChannels > 2) {
        ma_decoder_uninit(&dec);
        if (!open(path, 2, dec)) return {nullptr, "cannot read " + name};
    }

    auto sample = std::make_shared<Sample>();
    sample->channels = static_cast<int>(dec.outputChannels);
    sample->sample_rate = dec.outputSampleRate;
    sample->path = path;

    const auto max_frames = static_cast<uint64_t>(max_seconds * dec.outputSampleRate);
    constexpr ma_uint64 kChunk = 4096;
    std::string error;
    for (;;) {
        // Reading in chunks: an MP3's length is often unknown until the end.
        const std::size_t old_size = sample->frames.size();
        sample->frames.resize(old_size + kChunk * sample->channels);
        ma_uint64 read = 0;
        const ma_result r = ma_decoder_read_pcm_frames(&dec, sample->frames.data() + old_size, kChunk, &read);
        sample->frames.resize(old_size + read * sample->channels);
        if (static_cast<uint64_t>(sample->frame_count()) > max_frames) {
            error = name + " is longer than " + std::to_string(static_cast<int>(std::round(max_seconds / 60))) +
                    " minutes";
            break;
        }
        if (r != MA_SUCCESS || read < kChunk) break;  // MA_AT_END or a read error ends the file
    }
    ma_decoder_uninit(&dec);

    if (!error.empty()) return {nullptr, error};
    if (sample->frames.empty()) return {nullptr, name + " is empty"};
    sample->frames.shrink_to_fit();
    return {std::move(sample), {}};
}

}  // namespace aloglas
