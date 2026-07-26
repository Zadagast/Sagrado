// The Sagrado software framebuffer. Everything on screen is drawn into this
// 32-bit pixel buffer and blitted to the window in one call, the same way
// Haxial's own applications rendered (their only GDI import is
// SetDIBitsToDevice).
#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

#include "font.h"
#include "hap.h"

struct Color {
    uint8_t r, g, b;
};

constexpr uint32_t pack(Color c) {
    return (uint32_t(c.r) << 16) | (uint32_t(c.g) << 8) | uint32_t(c.b);
}

struct Rect {
    int x, y, w, h;
    int right() const { return x + w; }
    int bottom() const { return y + h; }
    bool contains(int px, int py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
};

class Canvas {
  public:
    void resize(int w, int h) {
        width_ = w;
        height_ = h;
        pixels_.assign(size_t(w) * size_t(h), 0);
    }
    int width() const { return width_; }
    int height() const { return height_; }
    const uint32_t *data() const { return pixels_.data(); }

    void put(int x, int y, uint32_t c) {
        if (x >= 0 && x < width_ && y >= 0 && y < height_)
            pixels_[size_t(y) * width_ + x] = c;
    }

    void fill(Rect r, Color c) {
        uint32_t p = pack(c);
        int x0 = r.x < 0 ? 0 : r.x, y0 = r.y < 0 ? 0 : r.y;
        int x1 = r.right() > width_ ? width_ : r.right();
        int y1 = r.bottom() > height_ ? height_ : r.bottom();
        for (int y = y0; y < y1; ++y) {
            uint32_t *row = pixels_.data() + size_t(y) * width_;
            for (int x = x0; x < x1; ++x) row[x] = p;
        }
    }

    void hline(int x0, int x1, int y, Color c) { fill({x0, y, x1 - x0, 1}, c); }
    void vline(int x, int y0, int y1, Color c) { fill({x, y0, 1, y1 - y0}, c); }

    void frame(Rect r, Color c) {
        hline(r.x, r.right(), r.y, c);
        hline(r.x, r.right(), r.bottom() - 1, c);
        vline(r.x, r.y, r.bottom(), c);
        vline(r.right() - 1, r.y, r.bottom(), c);
    }

    // Vertical gradient, one solid color per row.
    void vgradient(Rect r, Color top, Color bottom) {
        if (r.h <= 1) {
            fill(r, top);
            return;
        }
        for (int i = 0; i < r.h; ++i) {
            float t = float(i) / float(r.h - 1);
            Color c{uint8_t(top.r + (bottom.r - top.r) * t),
                    uint8_t(top.g + (bottom.g - top.g) * t),
                    uint8_t(top.b + (bottom.b - top.b) * t)};
            hline(r.x, r.right(), r.y + i, c);
        }
    }

    // Blit a theme image 1:1, honoring per-pixel transparency.
    void blit_image(const ThemeImage &img, int dx, int dy) {
        for (int y = 0; y < img.h; ++y)
            for (int x = 0; x < img.w; ++x) {
                uint32_t p = img.at(x, y);
                if (p >> 24) put(dx + x, dy + y, p & 0x00ffffff);
            }
    }

    // 9-slice a theme image into a rect using its authored caps: corners
    // 1:1, edges and center stretched (nearest-neighbor).
    void nine_slice(const ThemeImage &img, Rect r) {
        if (img.w <= 0 || img.h <= 0 || r.w <= 0 || r.h <= 0) return;
        int cl = img.caps[0], ct = img.caps[1];
        int cr = img.caps[2], cb = img.caps[3];
        if (cl + cr >= img.w) { cl = img.w / 2; cr = img.w - 1 - cl; }
        if (ct + cb >= img.h) { ct = img.h / 2; cb = img.h - 1 - ct; }
        if (cl + cr >= r.w) { cl = r.w / 2; cr = r.w - cl; }
        if (ct + cb >= r.h) { ct = r.h / 2; cb = r.h - ct; }
        int mid_sw = img.w - cl - cr, mid_sh = img.h - ct - cb;
        int mid_dw = r.w - cl - cr, mid_dh = r.h - ct - cb;
        for (int y = 0; y < r.h; ++y) {
            int sy = y < ct               ? y
                     : y >= r.h - cb      ? img.h - (r.h - y)
                     : mid_sh <= 0        ? ct
                                          : ct + (y - ct) * mid_sh / mid_dh;
            if (sy < 0 || sy >= img.h) continue;
            for (int x = 0; x < r.w; ++x) {
                int sx = x < cl          ? x
                         : x >= r.w - cr ? img.w - (r.w - x)
                         : mid_sw <= 0   ? cl
                                         : cl + (x - cl) * mid_sw / mid_dw;
                if (sx < 0 || sx >= img.w) continue;
                uint32_t p = img.at(sx, sy);
                if (p >> 24) put(r.x + x, r.y + y, p & 0x00ffffff);
            }
        }
    }

    int text_width(const char *s) const {
        int w = 0;
        for (; *s; ++s)
            if (*s >= 32 && *s < 127) w += kFont[*s - 32].advance;
        return w;
    }

    // Draw text with the built-in KDX pixel font. Returns the end x.
    int text(int x, int y, const char *s, Color c) {
        uint32_t p = pack(c);
        for (; *s; ++s) {
            if (*s < 32 || *s >= 127) continue;
            const Glyph &g = kFont[*s - 32];
            for (int row = 0; row < kFontHeight; ++row) {
                uint16_t bits = g.rows[row];
                for (int col = 0; bits; ++col, bits >>= 1)
                    if (bits & 1) put(x + col, y + row, p);
            }
            x += g.advance;
        }
        return x;
    }

  private:
    int width_ = 0, height_ = 0;
    std::vector<uint32_t> pixels_;
};
