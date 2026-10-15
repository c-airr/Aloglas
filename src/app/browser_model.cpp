#include "app/browser_model.h"

#include <algorithm>
#include <system_error>

#include "core/natural_sort.h"

namespace aloglas {

namespace {

std::string utf8(const std::filesystem::path& p) {
    const std::u8string s = p.u8string();
    return {s.begin(), s.end()};
}

// "C:/Samples/" and "C:/Samples/." both become "C:/Samples".
std::filesystem::path clean(const std::filesystem::path& p) {
    std::filesystem::path n = p.lexically_normal();
    if (!n.has_filename() && n.has_relative_path()) n = n.parent_path();
    return n;
}

}  // namespace

bool is_audio_file(const std::filesystem::path& path) {
    std::string ext = utf8(path.extension());
    std::transform(ext.begin(), ext.end(), ext.begin(), [](char c) {
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
    });
    return ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac" || ext == ".mp3";
}

bool BrowserModel::add_root(const std::filesystem::path& path) {
    const std::filesystem::path p = clean(path);
    for (const BrowserNode& r : roots_) {
        if (r.path == p) return false;
    }
    BrowserNode root;
    root.path = p;
    root.folder = true;
    root.name = p.has_filename() ? utf8(p.filename()) : utf8(p);  // a drive root has no folder name
    std::error_code ec;
    if (!std::filesystem::is_directory(p, ec)) {
        root.missing = true;
        log_.add(LogLevel::Warning, "Folder not found: " + utf8(p));
    }
    roots_.push_back(std::move(root));
    rebuild();
    return true;
}

void BrowserModel::remove_root(std::size_t root_index) {
    if (root_index >= roots_.size()) return;
    roots_.erase(roots_.begin() + static_cast<std::ptrdiff_t>(root_index));
    rebuild();
}

void BrowserModel::set_expanded(std::size_t row, bool expanded) {
    if (row >= rows_.size()) return;
    BrowserNode& node = *rows_[row].node;
    if (!node.folder || node.missing || node.expanded == expanded) return;
    if (expanded && !node.listed) list(node);
    node.expanded = expanded;
    rebuild();
}

std::vector<std::filesystem::path> BrowserModel::root_paths() const {
    std::vector<std::filesystem::path> v;
    for (const BrowserNode& r : roots_) v.push_back(r.path);
    return v;
}

void BrowserModel::list(BrowserNode& folder) {
    folder.listed = true;
    folder.children.clear();
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::directory_iterator it(folder.path, fs::directory_options::skip_permission_denied, ec);
    for (; !ec && it != fs::directory_iterator(); it.increment(ec)) {
        std::error_code type_ec;
        const bool dir = it->is_directory(type_ec);
        if (!dir && !(it->is_regular_file(type_ec) && is_audio_file(it->path()))) continue;
        BrowserNode child;
        child.path = it->path();
        child.name = utf8(it->path().filename());
        child.folder = dir;
        folder.children.push_back(std::move(child));
    }
    if (ec) log_.add(LogLevel::Warning, "Cannot list " + utf8(folder.path) + ": " + ec.message());
    std::sort(folder.children.begin(), folder.children.end(), [](const BrowserNode& a, const BrowserNode& b) {
        if (a.folder != b.folder) return a.folder;
        return natural_less(a.name, b.name);
    });
}

void BrowserModel::rebuild() {
    rows_.clear();
    // Depth-first walk over the expanded folders.
    std::function<void(BrowserNode&, int, int)> walk = [&](BrowserNode& n, int depth, int root) {
        rows_.push_back({&n, depth, root});
        if (!n.expanded) return;
        for (BrowserNode& c : n.children) walk(c, depth + 1, root);
    };
    for (std::size_t i = 0; i < roots_.size(); ++i) walk(roots_[i], 0, static_cast<int>(i));
    if (on_change) on_change();
}

}  // namespace aloglas
