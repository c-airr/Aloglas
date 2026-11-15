// What a drag between panels carries. The router owns one drag at a time, so
// a drop target can tell a browser file from a sound without guessing from
// the pointer. Later steps add their own kinds here.
#pragma once

#include <cstdint>
#include <filesystem>

namespace aloglas {

enum class DragKind { None, SampleFile, SoundRef, LoopRef };

struct DragPayload {
    DragKind kind = DragKind::None;
    std::filesystem::path path;  // SampleFile
    uint64_t sound_id = 0;       // SoundRef

    explicit operator bool() const { return kind != DragKind::None; }
};

}  // namespace aloglas
