#include "app/channel_rack.h"

#include <algorithm>
#include <cmath>

#include "ui/canvas.h"
#include "ui/number_format.h"
#include "ui/theme.h"

namespace aloglas {

namespace {
constexpr float kPad = 4;
constexpr float kMuteWidth = 18;
constexpr float kVolumeWidth = 45;
constexpr float kPanWidth = 34;
constexpr float kGap = 3;
}

ChannelRack::ChannelRack(Controller& controller, SampleLoader& loader, SampleCache& cache,
                         Log& log, PopupMenu& popup)
    : controller_(controller), loader_(loader), cache_(cache), log_(log), popup_(popup) {}

void ChannelRack::add_sample(const std::filesystem::path& path) {
    if (controller_.doc().sounds.size() >= 64) {
        log_.add(LogLevel::Warning, "Channel Rack is full (64 sounds)");
        return;
    }
    if (auto sample = cache_.find(path)) {
        selected_ = controller_.add_sound(path, std::move(sample));
        if (!controller_.doc().sounds.empty()) reveal(controller_.doc().sounds.size() - 1);
        invalidate();
        return;
    }
    const uint64_t request = loader_.request(path, false);
    pending_.insert(request);
}

void ChannelRack::on_load_result(const LoadResult& result) {
    if (result.preview || pending_.erase(result.id) == 0) return;
    if (!result.sample) {
        const std::u8string filename = result.path.filename().u8string();
        log_.add(LogLevel::Error, "Cannot add " + std::string(filename.begin(), filename.end()) + ": " + result.error);
        return;
    }
    cache_.put(result.sample);
    selected_ = controller_.add_sound(result.path, result.sample);
    if (selected_ != 0) {
        reveal(controller_.doc().sounds.size() - 1);
        const std::u8string filename = result.path.filename().u8string();
        log_.add(LogLevel::Info, "Added " + std::string(filename.begin(), filename.end()));
    }
    invalidate();
}

void ChannelRack::layout(Rect r) {
    rect = r;
    const int visible = std::max(1, static_cast<int>(rect.h / theme().row_height));
    const int count = static_cast<int>(controller_.doc().sounds.size());
    first_ = std::clamp(first_, 0, std::max(0, count - visible));
}

int ChannelRack::row_at(float y) const {
    const int row = first_ + static_cast<int>(std::floor((y - rect.y) / theme().row_height));
    return row >= 0 && row < static_cast<int>(controller_.doc().sounds.size()) ? row : -1;
}

void ChannelRack::select(std::size_t index, bool audition) {
    const auto& sounds = controller_.doc().sounds;
    if (index >= sounds.size()) return;
    selected_ = sounds[index].id;
    reveal(index);
    if (audition) controller_.preview_note(selected_, sounds[index].root);
    invalidate();
}

void ChannelRack::reveal(std::size_t index) {
    const int visible = std::max(1, static_cast<int>(rect.h / theme().row_height));
    if (static_cast<int>(index) < first_) first_ = static_cast<int>(index);
    else if (static_cast<int>(index) >= first_ + visible) first_ = static_cast<int>(index) - visible + 1;
}

void ChannelRack::remove_selected() {
    if (selected_ == 0) return;
    const auto& sounds = controller_.doc().sounds;
    const auto it = std::find_if(sounds.begin(), sounds.end(), [this](const Sound& s) { return s.id == selected_; });
    if (it == sounds.end()) return;
    const std::size_t index = static_cast<std::size_t>(it - sounds.begin());
    controller_.remove_sound(selected_);
    const auto& remaining = controller_.doc().sounds;
    selected_ = remaining.empty() ? 0 : remaining[std::min(index, remaining.size() - 1)].id;
    invalidate();
}

void ChannelRack::draw(Canvas& c) {
    const Theme& t = theme();
    c.push_clip(rect);
    const auto& sounds = controller_.doc().sounds;
    const float row_height = t.row_height;
    const int visible = std::max(1, static_cast<int>(rect.h / row_height) + 1);
    const int end = std::min(static_cast<int>(sounds.size()), first_ + visible);
    for (int i = first_; i < end; ++i) {
        const Sound& sound = sounds[static_cast<std::size_t>(i)];
        const float y = rect.y + static_cast<float>(i - first_) * row_height;
        const Rect row{rect.x, y, rect.w, row_height};
        const bool chosen = sound.id == selected_;
        c.fill_rect(row, chosen ? (has_focus() ? t.selection : with_alpha(t.selection, 45))
                                : ((i & 1) ? t.lane_alt : t.lane));
        const Color sound_color = t.sound_colors[static_cast<std::size_t>((sound.color % 12 + 12) % 12)];
        c.fill_rect({row.x + kPad, y + 3, 3, std::max(1.0f, row.h - 6)}, sound_color);
        c.fill_circle({row.x + kPad + 10, y + row.h / 2}, 4, sound.muted ? t.text_dim : t.accent);

        const float pan_x = row.right() - kPad - kPanWidth;
        const float volume_x = pan_x - kGap - kVolumeWidth;
        const float name_x = row.x + kPad + kMuteWidth;
        const float name_w = std::max(0.0f, volume_x - kGap - name_x);
        c.text({name_x, y, name_w, row.h}, sound.name, sound.muted ? t.text_dim : t.text,
               t.font_size_small, Canvas::Align::Left, chosen);
        c.fill_rect({volume_x, y + 2, kVolumeWidth, row.h - 4}, t.field_bg);
        c.stroke_rect({volume_x, y + 2, kVolumeWidth, row.h - 4}, t.panel_border);
        c.text({volume_x, y + 1, kVolumeWidth, row.h - 2},
               format_number({-60, 6, 0}, sound.volume_db), t.text_dim, t.font_size_small, Canvas::Align::Center);
        c.fill_rect({pan_x, y + 2, kPanWidth, row.h - 4}, t.field_bg);
        c.stroke_rect({pan_x, y + 2, kPanWidth, row.h - 4}, t.panel_border);
        const std::string pan = sound.pan < -0.01f ? "L" : sound.pan > 0.01f ? "R" : "C";
        c.text({pan_x, y + 1, kPanWidth, row.h - 2}, pan, t.text_dim, t.font_size_small, Canvas::Align::Center);
    }
    if (sounds.empty()) {
        c.text({rect.x + 8, rect.y + 4, rect.w - 16, t.row_height}, "Press Enter on a browser sample to add it",
               t.text_dim, t.font_size_small);
    }
    c.pop_clip();
}

bool ChannelRack::on_mouse_down(const MouseEvent& e) {
    focus();
    const int row = row_at(e.pos.y);
    if (row < 0) return true;
    const Sound& sound = controller_.doc().sounds[static_cast<std::size_t>(row)];
    selected_ = sound.id;
    const float pan_x = rect.right() - kPad - kPanWidth;
    const float volume_x = pan_x - kGap - kVolumeWidth;
    const float mute_right = rect.x + kPad + kMuteWidth;
    if (e.button == MouseButton::Right) {
        popup_.open(e.pos, {"Duplicate", "Delete"}, [this, id = sound.id](int choice) {
            if (choice == 0) {
                selected_ = controller_.duplicate_sound(id);
                invalidate();
            } else if (choice == 1) {
                selected_ = id;
                remove_selected();
            }
        });
        invalidate();
        return true;
    }
    if (e.button != MouseButton::Left) return true;
    if (e.pos.x < mute_right) {
        controller_.set_sound_muted(sound.id, !sound.muted);
    } else if (e.pos.x >= volume_x && e.pos.x < pan_x) {
        const float fraction = std::clamp((e.pos.x - volume_x) / kVolumeWidth, 0.0f, 1.0f);
        controller_.set_sound_volume(sound.id, -60.0f + fraction * 66.0f);
    } else if (e.pos.x >= pan_x) {
        const float fraction = std::clamp((e.pos.x - pan_x) / kPanWidth, 0.0f, 1.0f);
        controller_.set_sound_pan(sound.id, fraction * 2.0f - 1.0f);
    } else {
        controller_.preview_note(sound.id, sound.root);
    }
    invalidate();
    return true;
}

bool ChannelRack::on_wheel(const WheelEvent& e) {
    const int visible = std::max(1, static_cast<int>(rect.h / theme().row_height));
    const int count = static_cast<int>(controller_.doc().sounds.size());
    first_ = std::clamp(first_ - static_cast<int>(std::lround(e.dy * 3)), 0, std::max(0, count - visible));
    invalidate();
    return true;
}

bool ChannelRack::on_key(const KeyEvent& e) {
    const auto& sounds = controller_.doc().sounds;
    if (sounds.empty()) return false;
    auto selected = std::find_if(sounds.begin(), sounds.end(), [this](const Sound& s) { return s.id == selected_; });
    int index = selected == sounds.end() ? 0 : static_cast<int>(selected - sounds.begin());
    if (!e.mods.ctrl && !e.mods.alt && e.key == Key::Up) {
        index = std::max(0, index - 1);
        select(static_cast<std::size_t>(index), false);
        return true;
    }
    if (!e.mods.ctrl && !e.mods.alt && e.key == Key::Down) {
        index = std::min(static_cast<int>(sounds.size()) - 1, index + 1);
        select(static_cast<std::size_t>(index), false);
        return true;
    }
    if (!e.mods.ctrl && !e.mods.alt && e.key == Key::Enter) {
        select(static_cast<std::size_t>(index), true);
        return true;
    }
    if (!e.mods.ctrl && !e.mods.alt && e.key == Key::Delete) {
        remove_selected();
        return true;
    }
    return false;
}

}  // namespace aloglas
