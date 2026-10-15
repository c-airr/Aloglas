#include <doctest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "app/browser_model.h"
#include "app/log.h"

using namespace aloglas;
namespace fs = std::filesystem;

namespace {
struct TempTree {
    fs::path root;
    explicit TempTree(const char* name) : root(fs::temp_directory_path() / name) {
        fs::remove_all(root);
        fs::create_directories(root);
    }
    ~TempTree() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }
    void file(const fs::path& rel) {
        fs::create_directories((root / rel).parent_path());
        std::ofstream(root / rel) << "x";
    }
    void dir(const fs::path& rel) { fs::create_directories(root / rel); }
};

std::vector<std::string> names(const BrowserModel& m) {
    std::vector<std::string> v;
    for (const BrowserRow& r : m.rows()) v.push_back(std::string(r.depth * 2, ' ') + r.node->name);
    return v;
}

int warnings(const Log& log) {
    int n = 0;
    for (const LogLine& l : log.lines()) n += l.level == LogLevel::Warning;
    return n;
}
}  // namespace

TEST_CASE("audio extensions") {
    CHECK(is_audio_file("a.WAV"));
    CHECK(is_audio_file("a.Aiff"));
    CHECK(is_audio_file("a.aif"));
    CHECK(is_audio_file("a.flac"));
    CHECK(is_audio_file("a.mp3"));
    CHECK_FALSE(is_audio_file("a.ogg"));
    CHECK_FALSE(is_audio_file("a.txt"));
    CHECK_FALSE(is_audio_file("wav"));
}

TEST_CASE("folders first, natural order, audio only") {
    TempTree t("aloglas_browser_sort");
    t.dir("B");
    t.dir("a");
    t.file("Kick 10.wav");
    t.file("Kick 2.wav");
    t.file("notes.txt");
    t.file("hat.MP3");
    Log log;
    BrowserModel m(log);
    int changes = 0;
    m.on_change = [&] { ++changes; };
    CHECK(m.add_root(t.root));
    REQUIRE(m.rows().size() == 1);
    CHECK(m.rows()[0].depth == 0);
    CHECK(m.rows()[0].node->folder);
    m.set_expanded(0, true);
    CHECK(names(m) == std::vector<std::string>{"aloglas_browser_sort", "  a", "  B", "  hat.MP3", "  Kick 2.wav",
                                               "  Kick 10.wav"});
    CHECK(m.rows()[3].root_index == 0);
    CHECK_FALSE(m.rows()[3].node->folder);
    CHECK(m.rows()[3].node->path == t.root / "hat.MP3");
    CHECK(changes == 2);
}

TEST_CASE("collapse hides children; expand again doesn't relist") {
    TempTree t("aloglas_browser_collapse");
    t.file("one.wav");
    Log log;
    BrowserModel m(log);
    m.add_root(t.root);
    m.set_expanded(0, true);
    CHECK(m.rows().size() == 2);
    m.set_expanded(0, false);
    CHECK(m.rows().size() == 1);
    t.file("two.wav");
    m.set_expanded(0, true);
    CHECK(m.rows().size() == 2);
}

TEST_CASE("lists non-ASCII names") {
    TempTree t("aloglas_browser_utf8");
    t.file(fs::path(u8"Sample żółć") / u8"Kick ą.wav");
    Log log;
    BrowserModel m(log);
    m.add_root(t.root);
    m.set_expanded(0, true);
    REQUIRE(m.rows().size() == 2);
    CHECK(m.rows()[1].node->name == "Sample \xC5\xBC\xC3\xB3\xC5\x82\xC4\x87");
    m.set_expanded(1, true);
    REQUIRE(m.rows().size() == 3);
    CHECK(m.rows()[2].node->name == "Kick \xC4\x85.wav");
    CHECK(fs::exists(m.rows()[2].node->path));
}

TEST_CASE("missing root is dim and warns once") {
    Log log;
    BrowserModel m(log);
    CHECK(m.add_root(fs::temp_directory_path() / "aloglas_no_such_folder_xyz"));
    REQUIRE(m.rows().size() == 1);
    CHECK(m.rows()[0].node->missing);
    CHECK(warnings(log) == 1);
    m.set_expanded(0, true);
    CHECK(m.rows().size() == 1);
    CHECK(warnings(log) == 1);
    m.remove_root(0);
    CHECK(m.rows().empty());
}

TEST_CASE("duplicate roots are ignored") {
    TempTree t("aloglas_browser_dup");
    Log log;
    BrowserModel m(log);
    CHECK(m.add_root(t.root));
    CHECK_FALSE(m.add_root(t.root / "."));
    CHECK(m.rows().size() == 1);
    CHECK(m.root_paths().size() == 1);
}

TEST_CASE("unlistable folder logs and shows empty") {
    TempTree t("aloglas_browser_gone");
    t.dir("sub");
    Log log;
    BrowserModel m(log);
    m.add_root(t.root);
    m.set_expanded(0, true);
    REQUIRE(m.rows().size() == 2);
    fs::remove_all(t.root / "sub");
    m.set_expanded(1, true);
    CHECK(m.rows().size() == 2);
    CHECK(m.rows()[1].node->expanded);
    REQUIRE(warnings(log) == 1);
    CHECK(log.lines().back().text.find("Cannot list") != std::string::npos);
}

TEST_CASE("remove_root keeps the other roots") {
    TempTree a("aloglas_browser_ra");
    TempTree b("aloglas_browser_rb");
    Log log;
    BrowserModel m(log);
    m.add_root(a.root);
    m.add_root(b.root);
    m.remove_root(0);
    REQUIRE(m.rows().size() == 1);
    CHECK(m.rows()[0].node->path == b.root);
    CHECK(m.rows()[0].root_index == 0);
}
