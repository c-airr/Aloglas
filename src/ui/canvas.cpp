#include "ui/canvas.h"

#include <algorithm>
#include <cmath>

#include <nanovg.h>

#include "ui/text_fit.h"

namespace aloglas {

namespace {
NVGcolor to_nvg(Color c) {
    return nvgRGBA(c.r, c.g, c.b, c.a);
}

Rect intersect(Rect a, Rect b) {
    const float x0 = std::max(a.x, b.x);
    const float y0 = std::max(a.y, b.y);
    const float x1 = std::min(a.right(), b.right());
    const float y1 = std::min(a.bottom(), b.bottom());
    return {x0, y0, std::max(0.0f, x1 - x0), std::max(0.0f, y1 - y0)};
}
}  // namespace

Canvas::Canvas(NVGcontext* vg, int font_regular, int font_bold, float pixel_ratio)
    : vg_(vg), font_regular_(font_regular), font_bold_(font_bold), pixel_ratio_(pixel_ratio) {}

float Canvas::snap(float v) const {
    return std::round(v * pixel_ratio_) / pixel_ratio_;
}

void Canvas::fill_rect(Rect r, Color c, float radius) {
    if (r.empty()) return;
    nvgBeginPath(vg_);
    if (radius > 0) {
        nvgRoundedRect(vg_, r.x, r.y, r.w, r.h, radius);
    } else {
        nvgRect(vg_, r.x, r.y, r.w, r.h);
    }
    nvgFillColor(vg_, to_nvg(c));
    nvgFill(vg_);
}

void Canvas::stroke_rect(Rect r, Color c, float width) {
    if (r.empty()) return;
    // A stroke is centered on the path; inset by half the width so the
    // border stays inside `r` and lands on whole pixels.
    const float h = width / 2;
    nvgBeginPath(vg_);
    nvgRect(vg_, snap(r.x) + h, snap(r.y) + h, snap(r.w) - width, snap(r.h) - width);
    nvgStrokeColor(vg_, to_nvg(c));
    nvgStrokeWidth(vg_, width);
    nvgStroke(vg_);
}

void Canvas::hline(float x0, float x1, float y, Color c) {
    // One logical pixel = round(pixel_ratio) device pixels, drawn as a filled
    // rect on exact device-pixel boundaries: no antialiasing blur.
    const float thickness = std::max(1.0f, std::round(pixel_ratio_)) / pixel_ratio_;
    fill_rect({x0, snap(y), x1 - x0, thickness}, c);
}

void Canvas::vline(float x, float y0, float y1, Color c) {
    const float thickness = std::max(1.0f, std::round(pixel_ratio_)) / pixel_ratio_;
    fill_rect({snap(x), y0, thickness, y1 - y0}, c);
}

void Canvas::line(Point a, Point b, Color c, float width) {
    nvgBeginPath(vg_);
    nvgMoveTo(vg_, a.x, a.y);
    nvgLineTo(vg_, b.x, b.y);
    nvgStrokeColor(vg_, to_nvg(c));
    nvgStrokeWidth(vg_, width);
    nvgStroke(vg_);
}

void Canvas::fill_triangle(Point a, Point b, Point c, Color color) {
    nvgBeginPath(vg_);
    nvgMoveTo(vg_, a.x, a.y);
    nvgLineTo(vg_, b.x, b.y);
    nvgLineTo(vg_, c.x, c.y);
    nvgClosePath(vg_);
    nvgFillColor(vg_, to_nvg(color));
    nvgFill(vg_);
}

void Canvas::fill_circle(Point center, float radius, Color c) {
    nvgBeginPath(vg_);
    nvgCircle(vg_, center.x, center.y, radius);
    nvgFillColor(vg_, to_nvg(c));
    nvgFill(vg_);
}

void Canvas::stroke_circle(Point center, float radius, Color c, float width) {
    nvgBeginPath(vg_);
    nvgCircle(vg_, center.x, center.y, radius);
    nvgStrokeColor(vg_, to_nvg(c));
    nvgStrokeWidth(vg_, width);
    nvgStroke(vg_);
}

void Canvas::arc(Point center, float radius, float from_rad, float to_rad, Color c, float width) {
    nvgBeginPath(vg_);
    nvgArc(vg_, center.x, center.y, radius, from_rad, to_rad, NVG_CW);
    nvgStrokeColor(vg_, to_nvg(c));
    nvgStrokeWidth(vg_, width);
    nvgStroke(vg_);
}

void Canvas::set_font(float size, bool bold) {
    nvgFontFaceId(vg_, bold ? font_bold_ : font_regular_);
    nvgFontSize(vg_, size);
}

float Canvas::text_width(std::string_view s, float size, bool bold) {
    set_font(size, bold);
    return nvgTextBounds(vg_, 0, 0, s.data(), s.data() + s.size(), nullptr);
}

void Canvas::text(Rect r, std::string_view s, Color c, float size, Align align, bool bold) {
    if (s.empty() || r.w <= 0) return;
    const std::string fitted = fit_text(s, r.w, [&](std::string_view t) { return text_width(t, size, bold); });
    if (fitted.empty()) return;

    set_font(size, bold);
    int h_align = NVG_ALIGN_LEFT;
    float x = r.x;
    if (align == Align::Center) {
        h_align = NVG_ALIGN_CENTER;
        x = r.x + r.w / 2;
    } else if (align == Align::Right) {
        h_align = NVG_ALIGN_RIGHT;
        x = r.right();
    }
    nvgTextAlign(vg_, h_align | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg_, to_nvg(c));
    // Baseline on a whole pixel keeps small text sharp.
    nvgText(vg_, x, snap(r.y + r.h / 2), fitted.data(), fitted.data() + fitted.size());
}

std::vector<CaretStop> Canvas::caret_stops(std::string_view s, float size, bool bold) {
    set_font(size, bold);
    nvgTextAlign(vg_, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    std::vector<NVGglyphPosition> glyphs(s.size() + 1);
    const int n = nvgTextGlyphPositions(vg_, 0, 0, s.data(), s.data() + s.size(), glyphs.data(),
                                        static_cast<int>(glyphs.size()));
    std::vector<CaretStop> stops;
    stops.reserve(static_cast<std::size_t>(n) + 1);
    for (int i = 0; i < n; ++i) {
        stops.push_back({static_cast<std::size_t>(glyphs[i].str - s.data()), glyphs[i].x});
    }
    stops.push_back({s.size(), text_width(s, size, bold)});
    return stops;
}

void Canvas::push_clip(Rect r) {
    if (!clips_.empty()) r = intersect(r, clips_.back());
    clips_.push_back(r);
    nvgScissor(vg_, r.x, r.y, r.w, r.h);
}

void Canvas::pop_clip() {
    if (clips_.empty()) return;
    clips_.pop_back();
    if (clips_.empty()) {
        nvgResetScissor(vg_);
    } else {
        const Rect r = clips_.back();
        nvgScissor(vg_, r.x, r.y, r.w, r.h);
    }
}

}  // namespace aloglas
