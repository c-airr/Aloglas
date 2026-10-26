#include "app/compile.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

#include "app/document.h"

namespace aloglas {

std::unique_ptr<Snapshot> compile_snapshot(const Document& document) {
    auto snapshot = std::make_unique<Snapshot>();
    snapshot->loop_length_ticks = loop_length_ticks(document);
    snapshot->sounds.reserve(document.sounds.size());
    std::unordered_set<uint64_t> ids;
    ids.reserve(document.sounds.size());

    for (const Sound& sound : document.sounds) {
        if (sound.id == 0 || !ids.insert(sound.id).second) continue;
        const float pan = std::isfinite(sound.pan) ? std::clamp(sound.pan, -1.0f, 1.0f) : 0.0f;
        const float volume_db = std::isfinite(sound.volume_db) ? std::clamp(sound.volume_db, -60.0f, 6.0f) : 0.0f;
        const float fine = std::isfinite(sound.fine) ? std::clamp(sound.fine, -100.0f, 100.0f) : 0.0f;
        const double glide = std::isfinite(sound.glide_ms) ? std::clamp(sound.glide_ms, 0.0, 2000.0) : 80.0;
        const float angle = (pan + 1.0f) * 0.7853981633974483f;
        snapshot->sounds.push_back({sound.id,
                                    sound.sample,
                                    std::pow(10.0f, volume_db / 20.0f),
                                    std::cos(angle),
                                    std::sin(angle),
                                    !sound.muted,
                                    std::clamp(sound.root, 0, 127),
                                    fine,
                                    sound.cut_itself,
                                    sound.gate,
                                    glide});
    }

    snapshot->loop_events.reserve(document.loop_notes.size());
    for (const Note& note : document.loop_notes) {
        if (!ids.contains(note.sound_id) || note.start_tick < 0 || note.length_ticks <= 0 || note.pitch < 0 || note.pitch > 127)
            continue;
        const int64_t start = std::min(note.start_tick, snapshot->loop_length_ticks);
        const int64_t available = snapshot->loop_length_ticks - start;
        const int64_t end = start + std::min(note.length_ticks, available);
        if (end <= start) continue;
        snapshot->loop_events.push_back({note.sound_id, start, end, note.pitch,
                                         std::isfinite(note.velocity) ? std::clamp(note.velocity, 0.0f, 1.0f) : 0.0f,
                                         note.slide});
    }
    std::stable_sort(snapshot->loop_events.begin(), snapshot->loop_events.end(), [](const NoteEvent& a, const NoteEvent& b) {
        return a.start_tick < b.start_tick;
    });
    return snapshot;
}

}  // namespace aloglas
