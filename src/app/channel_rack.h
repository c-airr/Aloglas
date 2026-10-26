#pragma once

#include <cstdint>
#include <unordered_set>

#include "app/controller.h"
#include "app/log.h"
#include "engine/snapshot.h"
#include "samples/sample_cache.h"
#include "samples/sample_loader.h"
#include "ui/widget.h"
#include "ui/widgets/popup_menu.h"

namespace aloglas {

// Compact channel list. Enter in the browser adds its selected sample here;
// clicking a channel auditions it and the small controls change mute, level,
// and pan through the Controller.
class ChannelRack : public Widget {
public:
    ChannelRack(Controller& controller, SampleLoader& loader, SampleCache& cache, Log& log, PopupMenu& popup);

    void add_sample(const std::filesystem::path& path);
    void on_load_result(const LoadResult& result);
    SoundId selected_sound() const { return selected_; }

    bool focusable() const override { return true; }
    void layout(Rect r) override;
    void draw(Canvas& c) override;
    bool on_mouse_down(const MouseEvent& e) override;
    bool on_wheel(const WheelEvent& e) override;
    bool on_key(const KeyEvent& e) override;

private:
    int row_at(float y) const;
    void select(std::size_t index, bool audition);
    void reveal(std::size_t index);
    void remove_selected();

    Controller& controller_;
    SampleLoader& loader_;
    SampleCache& cache_;
    Log& log_;
    PopupMenu& popup_;
    SoundId selected_ = 0;
    int first_ = 0;
    std::unordered_set<uint64_t> pending_;
};

}  // namespace aloglas
