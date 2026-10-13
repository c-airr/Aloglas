// Helpers for rendering the engine offline in tests.
#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include "engine/engine.h"

namespace aloglas::test {

// Renders `frames` stereo frames in blocks and returns the interleaved output.
inline std::vector<float> render_frames(Engine& e, int64_t frames, uint32_t block = 512) {
    std::vector<float> out(static_cast<size_t>(frames) * 2);
    int64_t done = 0;
    while (done < frames) {
        const auto n = static_cast<uint32_t>(std::min<int64_t>(block, frames - done));
        e.render(out.data() + done * 2, n);
        done += n;
    }
    return out;
}

// Same, but throws the audio away (for long spans where only timing matters).
inline void render_skip(Engine& e, int64_t frames, uint32_t block = 4096) {
    std::vector<float> scratch(static_cast<size_t>(block) * 2);
    int64_t done = 0;
    while (done < frames) {
        const auto n = static_cast<uint32_t>(std::min<int64_t>(block, frames - done));
        e.render(scratch.data(), n);
        done += n;
    }
}

inline std::vector<int64_t> click_frames(const Engine& e) {
    std::vector<int64_t> v;
    for (size_t i = 0; i < e.click_count(); ++i) v.push_back(e.click(i).frame);
    return v;
}

inline std::vector<bool> click_accents(const Engine& e) {
    std::vector<bool> v;
    for (size_t i = 0; i < e.click_count(); ++i) v.push_back(e.click(i).accent);
    return v;
}

}  // namespace aloglas::test
