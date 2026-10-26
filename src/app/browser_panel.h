// Body of the BROWSER panel: the folder tree. Clicking a file (or reaching
// it with the arrow keys) previews it. The header hosts "+ Folder" and the
// "Preview" switch, like the LOG panel's Clear button.
#pragma once

#include <filesystem>
#include <functional>

#include "app/browser_model.h"
#include "app/previewer.h"
#include "ui/widget.h"
#include "ui/widgets/button.h"
#include "ui/widgets/popup_menu.h"
#include "ui/widgets/toggle_button.h"
#include "ui/widgets/tree_view.h"

namespace aloglas {

class BrowserPanel : public Widget {
public:
    BrowserPanel(BrowserModel& model, Previewer& previewer, PopupMenu& popup);

    // Not children: the panel header hosts them.
    Button& add_folder_button() { return add_folder_; }
    ToggleButton& preview_button() { return preview_button_; }
    TreeView& tree() { return tree_; }

    bool preview_on() const { return preview_on_; }
    void set_preview_on(bool on);
    std::function<void(const std::filesystem::path&)> on_add_sample;

    void layout(Rect r) override;
    void draw(Canvas& c) override;

private:
    BrowserModel& model_;
    Previewer& previewer_;
    PopupMenu& popup_;
    TreeView tree_;
    Button add_folder_{"+ Folder"};
    ToggleButton preview_button_{"Preview"};
    bool preview_on_ = true;
};

}  // namespace aloglas
