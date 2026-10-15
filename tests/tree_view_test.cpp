#include <doctest.h>

#include <filesystem>
#include <utility>
#include <vector>

#include "support/fake_host.h"
#include "ui/theme.h"
#include "ui/widgets/popup_menu.h"
#include "ui/widgets/tree_view.h"

using namespace aloglas;
using namespace aloglas::test;

namespace {
// 0 folder (expanded, depth 0)
//   1 folder (collapsed, depth 1)
//   2 file (depth 1)
//   3 file (depth 1)
// 4 folder (collapsed, depth 0)
struct Fixture {
    Widget root;
    TreeView tree;
    EventRouter router{root};
    FakeHost host{router};
    std::vector<TreeRow> rows{{0, true, true, false, "Splice"},
                              {1, true, false, false, "808s"},
                              {1, false, false, false, "kick.wav"},
                              {1, false, false, false, "snare.wav"},
                              {0, true, false, false, "My Samples"}};
    std::vector<std::size_t> selected;
    std::vector<std::pair<std::size_t, bool>> expanded;
    std::vector<std::size_t> context;

    explicit Fixture(float height = 200) {
        root.rect = {0, 0, 400, 400};
        root.add(tree);
        tree.count = [this] { return rows.size(); };
        tree.row = [this](std::size_t i) { return rows[i]; };
        tree.on_select = [this](std::size_t i) { selected.push_back(i); };
        tree.on_expand = [this](std::size_t i, bool e) { expanded.emplace_back(i, e); };
        tree.on_context = [this](std::size_t i, Point) { context.push_back(i); };
        tree.layout({0, 0, 200, height});
        root.set_host(&host);
    }
    // Middle of row i's label, away from the triangle.
    Point at(int i) const { return {150, (i + 0.5f) * theme().row_height}; }
};
}  // namespace

TEST_CASE("tree click selects and reports") {
    Fixture f;
    click(f.router, f.at(2));
    CHECK(f.tree.selected() == 2);
    CHECK(f.selected == std::vector<std::size_t>{2});
    CHECK(f.router.focus() == &f.tree);
}

TEST_CASE("tree triangle and double-click expand folders") {
    Fixture f;
    click(f.router, {4 + 0 * 12 + 6, f.at(0).y});  // triangle of row 0 (expanded)
    REQUIRE(f.expanded.size() == 1);
    CHECK(f.expanded[0] == std::pair<std::size_t, bool>{0, false});
    click(f.router, {4 + 1 * 12 + 6, f.at(1).y});  // triangle of row 1 (collapsed)
    CHECK(f.expanded.back() == std::pair<std::size_t, bool>{1, true});
    click(f.router, f.at(4), {}, 2);  // double-click a folder label
    CHECK(f.expanded.back() == std::pair<std::size_t, bool>{4, true});
    click(f.router, f.at(2), {}, 2);  // double-click a file: no expand
    CHECK(f.expanded.size() == 3);
}

TEST_CASE("tree arrows move, expand, collapse and go to the parent") {
    Fixture f;
    click(f.router, f.at(2));
    f.selected.clear();
    key(f.router, Key::Down);
    CHECK(f.tree.selected() == 3);
    key(f.router, Key::Up);
    key(f.router, Key::Up);
    CHECK(f.tree.selected() == 1);
    CHECK(f.selected == std::vector<std::size_t>{3, 2, 1});
    key(f.router, Key::Right);  // collapsed folder → expand
    CHECK(f.expanded.back() == std::pair<std::size_t, bool>{1, true});
    key(f.router, Key::Down);
    key(f.router, Key::Down);
    CHECK(f.tree.selected() == 3);
    key(f.router, Key::Left);  // a file → its parent
    CHECK(f.tree.selected() == 0);
    key(f.router, Key::Right);  // expanded folder → first child
    CHECK(f.tree.selected() == 1);
    key(f.router, Key::Up);
    key(f.router, Key::Left);  // expanded folder → collapse
    CHECK(f.expanded.back() == std::pair<std::size_t, bool>{0, false});
    key(f.router, Key::Up);  // at the top: stays
    CHECK(f.tree.selected() == 0);
}

TEST_CASE("tree right click selects without previewing") {
    Fixture f;
    f.router.mouse_down({f.at(3), MouseButton::Right});
    f.router.mouse_up({f.at(3), MouseButton::Right});
    CHECK(f.tree.selected() == 3);
    CHECK(f.selected.empty());
    CHECK(f.context == std::vector<std::size_t>{3});
}

TEST_CASE("tree wheel scrolls and keys keep the selection visible") {
    Fixture f(3 * theme().row_height);  // three rows visible
    for (int i = 0; i < 25; ++i) f.rows.push_back({0, false, false, false, "x"});  // 30 rows
    f.tree.layout(f.tree.rect);
    f.router.wheel({{50, 10}, 0, -1});  // towards the user: further down
    CHECK(f.tree.first_visible() == 3);
    f.router.wheel({{50, 10}, 0, -100});
    CHECK(f.tree.first_visible() == 27);
    f.router.wheel({{50, 10}, 0, 100});
    CHECK(f.tree.first_visible() == 0);

    click(f.router, f.at(2));
    key(f.router, Key::Down);
    key(f.router, Key::Down);  // row 4: below the view
    CHECK(f.tree.first_visible() == 2);
}

TEST_CASE("popup chooses an item and closes") {
    Widget root;
    root.rect = {0, 0, 400, 300};
    PopupMenu popup;
    root.add(popup);
    popup.layout(root.rect);
    EventRouter router(root);
    FakeHost host(router);
    root.set_host(&host);
    int chosen = -1;
    popup.open({100, 100}, {"Remove", "Rename"}, [&](int i) { chosen = i; });
    CHECK(popup.is_open());
    const float ih = theme().row_height + 4;
    click(router, {110, 100 + 2 + ih * 1.5f});
    CHECK(chosen == 1);
    CHECK_FALSE(popup.is_open());
}

TEST_CASE("popup closes on a press outside or Escape without choosing") {
    Widget root;
    root.rect = {0, 0, 400, 300};
    PopupMenu popup;
    root.add(popup);
    popup.layout(root.rect);
    EventRouter router(root);
    FakeHost host(router);
    root.set_host(&host);
    int chosen = -1;
    popup.open({100, 100}, {"Remove"}, [&](int i) { chosen = i; });
    click(router, {350, 280});
    CHECK_FALSE(popup.is_open());
    popup.open({390, 290}, {"Remove"}, [&](int i) { chosen = i; });  // near the corner: moved inside
    CHECK(popup.menu_rect().right() <= 400);
    CHECK(popup.menu_rect().bottom() <= 300);
    key(router, Key::Escape);
    CHECK_FALSE(popup.is_open());
    CHECK(chosen == -1);
}

TEST_CASE("pick_folder is routed through the host") {
    Widget root;
    EventRouter router(root);
    FakeHost host(router);
    root.set_host(&host);
    std::filesystem::path got;
    root.host()->pick_folder([&](std::filesystem::path p) { got = std::move(p); });
    REQUIRE(host.folder_request);
    host.folder_request(std::filesystem::path(u8"C:/x/\u017C"));
    CHECK(got == std::filesystem::path(u8"C:/x/\u017C"));
}

// Final review: the selection is a row index, so expanding a folder above it
// left the highlight on an unrelated row. A triangle press now selects its
// folder (without previewing), so the selection follows what was touched.
TEST_CASE("tree triangle selects its folder without reporting") {
    Fixture f;
    click(f.router, f.at(3));
    f.selected.clear();
    click(f.router, {4 + 1 * 12 + 6, f.at(1).y});
    CHECK(f.tree.selected() == 1);
    CHECK(f.selected.empty());
}
