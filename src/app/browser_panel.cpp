#include "app/browser_panel.h"

#include "ui/canvas.h"
#include "ui/theme.h"

namespace aloglas {

BrowserPanel::BrowserPanel(BrowserModel& model, Previewer& previewer, PopupMenu& popup)
    : model_(model), previewer_(previewer), popup_(popup) {
    tree_.count = [this] { return model_.rows().size(); };
    tree_.row = [this](std::size_t i) {
        const BrowserRow& r = model_.rows()[i];
        return TreeRow{r.depth, r.node->folder, r.node->expanded, r.node->missing, r.node->name};
    };
    tree_.on_select = [this](std::size_t i) {
        const BrowserNode& n = *model_.rows()[i].node;
        if (!n.folder && preview_on_) previewer_.preview(n.path);
    };
    tree_.on_expand = [this](std::size_t i, bool expand) { model_.set_expanded(i, expand); };
    tree_.on_context = [this](std::size_t i, Point at) {
        const BrowserRow& r = model_.rows()[i];
        if (r.depth != 0) return;  // only added folders can be removed
        const int root = r.root_index;
        popup_.open(at, {"Remove"}, [this, root](int) { model_.remove_root(static_cast<std::size_t>(root)); });
    };
    model_.on_change = [this] {
        tree_.layout(tree_.rect);  // clamps the selection after a removal
        invalidate();
    };
    add(tree_);

    add_folder_.on_click = [this] {
        if (UiHost* h = host()) h->pick_folder([this](std::filesystem::path p) { model_.add_root(p); });
    };
    preview_button_.value = [this] { return preview_on_; };
    preview_button_.on_toggle = [this](bool on) { set_preview_on(on); };
}

void BrowserPanel::set_preview_on(bool on) {
    if (on == preview_on_) return;
    preview_on_ = on;
    if (!on) previewer_.stop();
    invalidate();
}

void BrowserPanel::layout(Rect r) {
    rect = r;
    tree_.layout(r);
}

void BrowserPanel::draw(Canvas& c) {
    c.fill_rect(rect, theme().panel);
    draw_children(c);
}

}  // namespace aloglas
