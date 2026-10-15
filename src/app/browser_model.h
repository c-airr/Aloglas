// What the BROWSER shows: the folders the user added, listed lazily (a
// folder is read from disk the first time it is expanded), with only
// sub-folders and audio files in it.
#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "app/log.h"

namespace aloglas {

bool is_audio_file(const std::filesystem::path& path);  // .wav .aif .aiff .flac .mp3, any case

struct BrowserNode {
    std::filesystem::path path;
    std::string name;  // UTF-8
    bool folder = false;
    bool expanded = false;
    bool listed = false;   // children read from disk
    bool missing = false;  // a root that doesn't exist (shown dim)
    std::vector<BrowserNode> children;
};

// One visible line of the tree. `node` stays valid until the next change.
struct BrowserRow {
    BrowserNode* node = nullptr;
    int depth = 0;       // 0 = the row is a root
    int root_index = 0;  // which root the row belongs to
};

class BrowserModel {
public:
    explicit BrowserModel(Log& log) : log_(log) {}

    // False when the folder is already there. A folder that doesn't exist is
    // still added (dim), so it comes back when the drive is plugged in again.
    bool add_root(const std::filesystem::path& path);
    void remove_root(std::size_t root_index);
    // Reads the folder from disk the first time it opens.
    void set_expanded(std::size_t row, bool expanded);

    const std::vector<BrowserRow>& rows() const { return rows_; }
    std::vector<std::filesystem::path> root_paths() const;

    std::function<void()> on_change;

private:
    void list(BrowserNode& folder);
    void rebuild();

    Log& log_;
    std::vector<BrowserNode> roots_;
    std::vector<BrowserRow> rows_;
};

}  // namespace aloglas
