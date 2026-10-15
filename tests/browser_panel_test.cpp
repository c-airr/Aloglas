#include <doctest.h>

#include <chrono>
#include <filesystem>
#include <thread>

#include "app/browser_model.h"
#include "app/browser_panel.h"
#include "app/log.h"
#include "app/previewer.h"
#include "engine/preview.h"
#include "samples/sample_cache.h"
#include "samples/sample_loader.h"
#include "support/fake_host.h"
#include "support/recording_sink.h"
#include "ui/theme.h"
#include "ui/widgets/popup_menu.h"

using namespace aloglas;
using namespace aloglas::test;
using namespace std::chrono_literals;
namespace fs = std::filesystem;

namespace {
fs::path data(const char* name) {
    return fs::path(ALOGLAS_TEST_DATA) / name;
}

// A folder with three short WAVs, laid out as the panel lists them.
struct Fixture {
    fs::path dir = fs::temp_directory_path() / "aloglas_browser_panel";
    RecordingSink sink;
    SampleLoader loader;
    SampleCache cache;
    Log log;
    Previewer previewer{sink, loader, cache, log};
    BrowserModel model{log};
    PopupMenu popup;
    BrowserPanel panel{model, previewer, popup};
    Widget root;
    EventRouter router{root};
    FakeHost host{router};

    Fixture() {
        fs::remove_all(dir);
        fs::create_directories(dir);
        for (const char* n : {"a.wav", "b.wav", "c.wav"}) fs::copy_file(data("sine_mono_44k.wav"), dir / n);
        root.rect = {0, 0, 400, 400};
        root.add(panel.add_folder_button());
        root.add(panel.preview_button());
        root.add(panel);
        root.add(popup);
        panel.add_folder_button().layout({0, 0, 58, 16});
        panel.preview_button().layout({60, 0, 52, 16});
        panel.layout({0, 20, 300, 300});
        popup.layout(root.rect);
        root.set_host(&host);
    }
    ~Fixture() {
        std::error_code ec;
        fs::remove_all(dir, ec);
    }

    Point row(int i) const { return {150, 20 + (i + 0.5f) * theme().row_height}; }
    void add_and_expand() {
        model.add_root(dir);
        model.set_expanded(0, true);
        panel.layout(panel.rect);
    }
    // Runs loader results through the previewer until one for `file` arrived.
    void deliver_until(const fs::path& file) {
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        bool done = false;
        while (!done && std::chrono::steady_clock::now() < deadline) {
            for (const LoadResult& r : loader.take_results()) {
                previewer.on_result(r);
                done |= r.path == file;
            }
            std::this_thread::sleep_for(5ms);
        }
        REQUIRE(done);
    }
    int previews() const {
        int n = 0;
        for (const Command& c : sink.sent) n += c.type == CommandType::PreviewSample;
        return n;
    }
};
}  // namespace

TEST_CASE("+ Folder adds the picked folder") {
    Fixture f;
    click(f.router, {29, 8});
    REQUIRE(f.host.folder_request);
    f.host.folder_request(f.dir);
    REQUIRE(f.model.rows().size() == 1);
    CHECK(f.model.rows()[0].node->path == f.dir);
}

TEST_CASE("clicking a file previews it") {
    Fixture f;
    f.add_and_expand();
    click(f.router, f.row(1));
    f.deliver_until(f.dir / "a.wav");
    CHECK(f.previews() == 1);
}

TEST_CASE("arrow keys preview each file they reach") {
    Fixture f;
    f.add_and_expand();
    click(f.router, f.row(1));  // a.wav
    key(f.router, Key::Down);   // b.wav
    key(f.router, Key::Down);   // c.wav
    f.deliver_until(f.dir / "c.wav");
    std::this_thread::sleep_for(30ms);
    for (const LoadResult& r : f.loader.take_results()) f.previewer.on_result(r);
    REQUIRE(f.previews() == 1);
    CHECK(f.sink.sent.back().preview->sample->path == f.dir / "c.wav");
}

TEST_CASE("preview off stops and stays quiet") {
    Fixture f;
    f.add_and_expand();
    CHECK(f.panel.preview_on());
    click(f.router, {86, 8});
    CHECK_FALSE(f.panel.preview_on());
    REQUIRE_FALSE(f.sink.sent.empty());
    CHECK(f.sink.sent.back().type == CommandType::StopPreview);
    click(f.router, f.row(1));
    std::this_thread::sleep_for(50ms);
    CHECK(f.loader.take_results().empty());
    CHECK(f.previews() == 0);
}

TEST_CASE("right click a root offers Remove") {
    Fixture f;
    f.add_and_expand();
    f.router.mouse_down({f.row(2), MouseButton::Right});  // a file: no menu
    f.router.mouse_up({f.row(2), MouseButton::Right});
    CHECK_FALSE(f.popup.is_open());
    f.router.mouse_down({f.row(0), MouseButton::Right});
    f.router.mouse_up({f.row(0), MouseButton::Right});
    REQUIRE(f.popup.is_open());
    const Rect m = f.popup.menu_rect();
    click(f.router, {m.x + 10, m.y + 2 + (theme().row_height + 4) / 2});
    CHECK_FALSE(f.popup.is_open());
    CHECK(f.model.rows().empty());
}
