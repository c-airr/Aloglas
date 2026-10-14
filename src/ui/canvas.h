// Drawing API for widgets: a thin layer over NanoVG with the primitives a DAW
// needs. Coordinates are logical pixels; NanoVG scales them to the screen.
#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include "ui/geometry.h"
#include "ui/theme.h"

struct NVGcontext;

namespace aloglas {

// x offset of a caret position (byte offset into a UTF-8 string).
struct CaretStop {
    std::size_t byte = 0;
    float x = 0;
};

class Canvas {
public:
    enum class Align { Left, Center, Right };

    Canvas(NVGcontext* vg, int font_regular, int font_bold, float pixel_ratio);

    float pixel_ratio() const { return pixel_ratio_; }

    void fill_rect(Rect r, Color c, float radius = 0);
    void stroke_rect(Rect r, Color c, float width = 1);
    // Crisp 1-px lines: snapped to the device pixel grid so they never blur
    // across two pixel rows (important for grids).
    void hline(float x0, float x1, float y, Color c);
    void vline(float x, float y0, float y1, Color c);
    void line(Point a, Point b, Color c, float width = 1);  // antialiased, any angle
    void fill_triangle(Point a, Point b, Point c, Color color);
    void fill_circle(Point center, float radius, Color c);
    void stroke_circle(Point center, float radius, Color c, float width = 1);
    void arc(Point center, float radius, float from_rad, float to_rad, Color c, float width);

    // Text vertically centered in `r`; shortened with "…" when too wide.
    void text(Rect r, std::string_view s, Color c, float size, Align align = Align::Left, bool bold = false);
    float text_width(std::string_view s, float size, bool bold = false);
    // One stop per code-point boundary, including the end (x relative to the text start).
    std::vector<CaretStop> caret_stops(std::string_view s, float size, bool bold = false);

    // Clips nest: each push intersects with the current clip.
    void push_clip(Rect r);
    void pop_clip();

private:
    void set_font(float size, bool bold);
    float snap(float v) const;

    NVGcontext* vg_;
    int font_regular_;
    int font_bold_;
    float pixel_ratio_;
    std::vector<Rect> clips_;
};

}  // namespace aloglas
