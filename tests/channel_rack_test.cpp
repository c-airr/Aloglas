// The channel rack: drops from the browser, Enter, clicks that audition,
// mute/volume/pan and deletion.
#include <doctest.h>

#include <filesystem>
#include <memory>

#include "app/browser_model.h"
#include "app/browser_panel.h"
#include "app/channel_rack.h"
#include "app/controller.h"
#include "app/log.h"
#include "app/previewer.h"
#include "samples/sample.h"
#include "samples/sample_cache.h"
#include "samples/sample_loader.h"
#include "support/fake_host.h"
#include "support/recording_sink.h"
#include "ui/theme.h"
#include "ui/widgets/popup_menu.h"

using namespace aloglas;
using namespace aloglas::test;
namespace fs = std::filesystem;

namespace {
fs::path data(const char* name) {
    return fs::path(ALOGLAS_TEST_DATA) / name;
}

std::shared_ptr<const Sample> cached(const fs::path& path) {
    auto s = std::make_shared<Sample>();
    s->frames.assign(48'000, 0.5f);
    s->sample_rate = 48000;
    s->path = path;
    return s;
}

MouseEvent at(Point p) {
    return MouseEvent{p, MouseButton::Left, 1, {}};
}

struct Fixture {
    fs::path dir = fs::temp_directory_path() / "aloglas_channel_rack";
    RecordingSink sink;
    Document doc;
    Controller controller{doc, sink};
    SampleCache cache;
    SampleLoader loader;
    Log log;
    Previewer previewer{sink, loader, cache, log};
    BrowserModel model{log};
    PopupMenu popup;
    BrowserPanel browser{model, previewer, popup};
    ChannelRack rack{controller, loader, cache, log, popup};
    // The cache holds weak pointers: these keep the decoded samples alive.
    std::shared_ptr<const Sample> sample_a{cached(dir / "a.wav")};
    std::shared_ptr<const Sample> sample_b{cached(dir / "b.wav")};
    Widget root;
    EventRouter router{root};
    FakeHost host{router};

    Fixture() {
        fs::remove_all(dir);
        fs::create_directories(dir);
        for (const char* n : {"a.wav", "b.wav"}) fs::copy_file(data("sine_mono_44k.wav"), dir / n);
        root.rect = {0, 0, 400, 300};
        root.add(browser);
        root.add(rack);
        root.add(popup);
        browser.layout({0, 0, 200, 300});
        rack.layout({210, 0, 190, 300});
        popup.layout(root.rect);
        root.set_host(&host);
        browser.set_preview_on(false);  // the loader is not part of these tests
        browser.on_add_sample = [this](const fs::path& p) { rack.add_sample(p); };
        model.add_root(dir);
        model.set_expanded(0, true);
        browser.layout(browser.rect);
        cache.put(sample_a);
        cache.put(sample_b);
    }
    ~Fixture() {
        std::error_code ec;
        fs::remove_all(dir, ec);
    }

    Point file_row(int i) const { return {150, (i + 0.5f) * theme().row_height}; }
    Point channel_row(float x) const { return {210 + x, theme().row_height / 2}; }};
}  // namespace

TEST_CASE("a sample dragged from the browser lands in the rack") {
    Fixture f;
    CHECK(f.doc.sounds.empty());
    f.router.mouse_down(at(f.file_row(1)));
    CHECK_FALSE(f.host.dragging);  // a press alone is not a drag yet
    f.router.mouse_move(at({f.file_row(1).x + 60, 150}));
    CHECK(f.host.dragging);
    f.router.mouse_up(at({300, 60}));
    REQUIRE(f.doc.sounds.size() == 1);
    CHECK(f.doc.sounds[0].sample_path == f.dir / "a.wav");
    CHECK(f.doc.sounds[0].name == "a.wav");
    CHECK(f.rack.selected_sound() == f.doc.sounds[0].id);
    CHECK_FALSE(f.host.dragging);
    CHECK(f.sink.sent.back().type == CommandType::SetSnapshot);
    REQUIRE(f.sink.sent.back().snapshot);
    CHECK(f.sink.sent.back().snapshot->sounds.size() == 1);
}

TEST_CASE("dropping a sample outside the rack adds nothing") {
    Fixture f;
    drag(f.router, f.file_row(1), {100, 250});  // back onto the browser tree
    CHECK(f.doc.sounds.empty());
    CHECK_FALSE(f.host.dragging);
}

TEST_CASE("folders are not draggable") {
    Fixture f;
    drag(f.router, f.file_row(0), {300, 60});  // the root folder
    CHECK(f.doc.sounds.empty());
}

TEST_CASE("Enter adds the selected browser sample") {
    Fixture f;
    f.router.set_focus(&f.browser.tree());
    click(f.router, f.file_row(2));  // b.wav
    key(f.router, Key::Enter);
    REQUIRE(f.doc.sounds.size() == 1);
    CHECK(f.doc.sounds[0].sample_path == f.dir / "b.wav");
}

TEST_CASE("clicking a channel auditions it") {
    Fixture f;
    drag(f.router, f.file_row(1), {300, 60});
    REQUIRE(f.doc.sounds.size() == 1);
    const SoundId id = f.doc.sounds[0].id;
    click(f.router, f.channel_row(60));
    REQUIRE_FALSE(f.sink.sent.empty());
    CHECK(f.sink.sent.back().type == CommandType::PreviewNote);
    CHECK(f.sink.sent.back().a == static_cast<int64_t>(id));
    CHECK(f.sink.sent.back().b == 60);
}

TEST_CASE("mute, volume and pan clicks update the document and the snapshot") {
    Fixture f;
    drag(f.router, f.file_row(1), {300, 60});
    REQUIRE(f.doc.sounds.size() == 1);

    click(f.router, f.channel_row(8));  // mute
    CHECK(f.doc.sounds[0].muted);
    REQUIRE(f.sink.sent.back().snapshot);
    CHECK_FALSE(f.sink.sent.back().snapshot->sounds[0].audible);

    click(f.router, {210 + 126.5f, 8});  // half of the volume field (45 px wide)
    CHECK(f.doc.sounds[0].volume_db == doctest::Approx(-27.0f).epsilon(0.02));

    click(f.router, {210 + 177.5f, 8});  // three quarters across the pan field (34 px)
    CHECK(f.doc.sounds[0].pan == doctest::Approx(0.5f).epsilon(0.02));
}

TEST_CASE("Delete removes the selected sound and its notes") {
    Fixture f;
    drag(f.router, f.file_row(1), {300, 60});
    REQUIRE(f.doc.sounds.size() == 1);
    Note note;
    note.sound_id = f.doc.sounds[0].id;
    REQUIRE(f.controller.add_note(note));
    REQUIRE(f.doc.loop_notes.size() == 1);

    click(f.router, f.channel_row(60));
    key(f.router, Key::Delete);
    CHECK(f.doc.sounds.empty());
    CHECK(f.doc.loop_notes.empty());
    REQUIRE(f.sink.sent.back().snapshot);
    CHECK(f.sink.sent.back().snapshot->sounds.empty());
}

TEST_CASE("right click duplicates the channel with a new id") {
    Fixture f;
    drag(f.router, f.file_row(1), {300, 60});
    REQUIRE(f.doc.sounds.size() == 1);
    const SoundId first = f.doc.sounds[0].id;

    f.router.mouse_down({f.channel_row(60), MouseButton::Right});
    f.router.mouse_up({f.channel_row(60), MouseButton::Right});
    REQUIRE(f.popup.is_open());
    const Rect menu = f.popup.menu_rect();
    click(f.router, {menu.x + 10, menu.y + 2 + (theme().row_height + 4) / 2});

    REQUIRE(f.doc.sounds.size() == 2);
    CHECK(f.doc.sounds[0].id == first);
    CHECK(f.doc.sounds[1].id != first);
    CHECK(f.doc.sounds[1].sample_path == f.doc.sounds[0].sample_path);
    CHECK(f.rack.selected_sound() == f.doc.sounds[1].id);
}
