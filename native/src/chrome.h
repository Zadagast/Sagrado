// The KDX Standard window chrome, drawn from pixel values measured off the
// real Haxial TextEdit: one solid red slab (title bar + borders) with the
// client area cut out, lit from the top-left.
#pragma once
#include "canvas.h"

// Standard color ramp (Haxial Standard color table entries 36..40).
constexpr Color kBlack{0, 0, 0};
constexpr Color kWhite{255, 255, 255};
constexpr Color kBright{204, 0, 0};  // #CC0000
constexpr Color kBody{136, 0, 0};    // #880000
constexpr Color kDeep{68, 0, 0};     // #440000
// The exact title gradients measured off the real window, rows 2..19.
constexpr uint8_t kTitleGrad[18] = {50,  61,  72,  82,  93,  104,
                                    114, 125, 136, 139, 146, 153,
                                    160, 167, 174, 181, 188, 195};
// Unfocused, the whole chrome goes greyscale (measured).
constexpr uint8_t kTitleGradGrey[18] = {12, 15, 18, 20, 23,  26,
                                        28, 31, 34, 39, 52,  65,
                                        78, 91, 104, 117, 130, 143};

constexpr Color kGreyBright{85, 85, 85};
constexpr Color kGreyBody{34, 34, 34};
constexpr Color kGreyDeep{17, 17, 17};

struct ChromeColors {
    Color bright, body, deep;
};
inline ChromeColors chrome_colors(bool focused) {
    return focused ? ChromeColors{kBright, kBody, kDeep}
                   : ChromeColors{kGreyBright, kGreyBody, kGreyDeep};
}

// Greys used by the menu/tab bars and scrollbars.
constexpr Color kBarLight{102, 102, 102}; // #666666
constexpr Color kBarBody{51, 51, 51};     // #333333
constexpr Color kBarDark{17, 17, 17};     // #111111
constexpr Color kTrack{34, 34, 34};       // #222222
constexpr Color kThumb{51, 51, 51};
constexpr Color kThumbHi{68, 68, 68};
constexpr Color kGlyphGrey{136, 136, 136};

// Standard metrics, measured from the real window.
constexpr int kTitleH = 22;
constexpr int kBorder = 6;
constexpr int kBtn = 14;      // title-bar boxes are 14x14
constexpr int kBtnTop = 4;    // ..4px below the window top
constexpr int kGrip = 20;     // grow box
constexpr int kScrollbar = 16;

struct ChromeLayout {
    Rect window;
    Rect client;
    Rect close_box;
    Rect hatch_box;
    Rect max_box;
    Rect min_box;
    Rect grip;
};

inline ChromeLayout chrome_layout(int w, int h) {
    ChromeLayout lay;
    lay.window = {0, 0, w, h};
    lay.client = {kBorder, kTitleH, w - 2 * kBorder, h - kTitleH - kBorder};
    lay.close_box = {5, kBtnTop, kBtn, kBtn};
    lay.hatch_box = {lay.close_box.right() + 9, kBtnTop, 32, kBtn};
    lay.min_box = {lay.client.right() - kBtn, kBtnTop, kBtn, kBtn};
    lay.max_box = {lay.min_box.x - 4 - kBtn, kBtnTop, kBtn, kBtn};
    lay.grip = {w - 1 - kGrip, h - 1 - kGrip, kGrip, kGrip};
    return lay;
}

// A Standard title-bar box: black border, red fill, bright/shadow bevel.
inline void bevel_box(Canvas &cv, Rect r, bool pressed,
                      ChromeColors cc = chrome_colors(true)) {
    cv.fill(r, pressed ? cc.deep : cc.body);
    cv.frame(r, kBlack);
    Color tl = pressed ? cc.deep : cc.bright;
    Color br = pressed ? cc.bright : cc.deep;
    cv.hline(r.x + 1, r.right() - 1, r.y + 1, tl);
    cv.vline(r.x + 1, r.y + 1, r.bottom() - 1, tl);
    cv.hline(r.x + 1, r.right() - 1, r.bottom() - 2, br);
    cv.vline(r.right() - 2, r.y + 1, r.bottom() - 1, br);
}

inline void diagonal_hatch(Canvas &cv, Rect r, Color c) {
    // 3px-wide stripes on a 7px period, like the real drag box.
    for (int i = -r.h - 7; i < r.w; i += 7)
        for (int t = 0; t < 3; ++t)
            for (int y = 0; y < r.h; ++y) {
                int x = i + t + y;
                if (x >= 0 && x < r.w) cv.put(r.x + x, r.y + y, pack(c));
            }
}

// The whole Standard frame: slab, gradient, lighting, client cutout, boxes.
inline void paint_chrome(Canvas &cv, const ChromeLayout &lay, const char *title,
                         bool focused, int hot_box, int pressed_box) {
    Rect win = lay.window;
    Rect client = lay.client;
    Rect slab = {1, 1, win.w - 2, win.h - 2};

    // Unfocused, the whole chrome goes greyscale like the real thing.
    ChromeColors cc = chrome_colors(focused);

    // Slab body, then the title gradient flowing into the side borders.
    cv.fill(slab, cc.body);
    for (int i = 0; i < 18; ++i) {
        uint8_t v = focused ? kTitleGrad[i] : kTitleGradGrey[i];
        cv.hline(slab.x, slab.right(), 2 + i,
                 focused ? Color{v, 0, 0} : Color{v, v, v});
    }
    cv.hline(slab.x, slab.right(), kTitleH - 2, cc.deep);

    // Bright faces (light from the top-left).
    cv.hline(slab.x, slab.right(), slab.y, cc.bright);
    cv.vline(slab.x, slab.y, slab.bottom(), cc.bright);
    cv.vline(client.right() + 1, client.y - 1, client.bottom() + 1, cc.bright);
    cv.hline(client.x - 1, client.right() + 2, client.bottom() + 1, cc.bright);

    // Shadow faces.
    cv.vline(client.x - 2, client.y - 2, slab.bottom(), cc.deep);
    cv.hline(client.x - 2, client.right() + 1, client.y - 2, cc.deep);
    cv.vline(slab.right() - 1, client.y - 2, slab.bottom(), cc.deep);
    cv.hline(slab.x + 1, slab.right(), slab.bottom() - 1, cc.deep);

    // The only interior black: the client-hole outline. Plus the outer edge.
    cv.frame({client.x - 1, client.y - 1, client.w + 2, client.h + 2}, kBlack);
    cv.frame(win, kBlack);

    // Title, centred, in the KDX pixel font (stays white, like the real).
    int tw = cv.text_width(title);
    cv.text((win.w - tw) / 2, (kTitleH - kFontHeight) / 2, title, kWhite);

    // Title-bar boxes: X, hatch drag stripes, + and -.
    bevel_box(cv, lay.close_box, pressed_box == 1, cc);
    {
        Rect r = lay.close_box;
        for (int i = 0; i < 6; ++i) {
            cv.put(r.x + 4 + i, r.y + 4 + i, pack(kWhite));
            cv.put(r.x + 5 + i, r.y + 4 + i, pack(kWhite));
            cv.put(r.x + 4 + i, r.y + 9 - i, pack(kWhite));
            cv.put(r.x + 5 + i, r.y + 9 - i, pack(kWhite));
        }
    }
    bevel_box(cv, lay.hatch_box, false, cc);
    diagonal_hatch(cv, {lay.hatch_box.x + 3, lay.hatch_box.y + 3,
                        lay.hatch_box.w - 6, lay.hatch_box.h - 6},
                   kWhite);
    bevel_box(cv, lay.max_box, pressed_box == 3, cc);
    cv.fill({lay.max_box.x + 2, lay.max_box.y + 6, 10, 3}, kWhite);
    cv.fill({lay.max_box.x + 6, lay.max_box.y + 2, 3, 10}, kWhite);
    bevel_box(cv, lay.min_box, pressed_box == 4, cc);
    cv.fill({lay.min_box.x + 2, lay.min_box.y + 6, 10, 3}, kWhite);

    (void)hot_box;
}

// The lower-right grow box: a red plate merging into the frame corner.
inline void paint_grip(Canvas &cv, Rect g, bool focused) {
    ChromeColors cc = chrome_colors(focused);
    cv.fill(g, cc.body);
    cv.hline(g.x, g.right(), g.y, kBlack);
    cv.vline(g.x, g.y, g.bottom(), kBlack);
    cv.hline(g.x + 1, g.right(), g.y + 1, cc.bright);
    cv.vline(g.x + 1, g.y + 1, g.bottom(), cc.bright);
    cv.hline(g.x + 1, g.right(), g.bottom() - 1, cc.deep);
    cv.vline(g.right() - 1, g.y + 1, g.bottom(), cc.deep);
    // Bright diagonal grip stripes toward the corner.
    for (int i = 0; i < 3; ++i) {
        int o = 4 + i * 4;
        for (int s = 0; s <= o; ++s) {
            cv.put(g.right() - 3 - o + s, g.bottom() - 3 - s, pack(cc.bright));
            cv.put(g.right() - 2 - o + s, g.bottom() - 3 - s, pack(cc.bright));
        }
    }
}

// A raised grey bar (menu bar / tab strip base).
inline void raised_bar(Canvas &cv, Rect r) {
    cv.fill(r, kBarBody);
    cv.hline(r.x, r.right(), r.y, kBarLight);
    cv.hline(r.x, r.right(), r.bottom() - 2, kBarDark);
    cv.hline(r.x, r.right(), r.bottom() - 1, kBlack);
}
