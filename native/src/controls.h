// Sagrado Kit dialog controls: text fields, checkboxes and push buttons,
// drawn into the framebuffer from the KDX Standard primitives (values
// measured pixel-by-pixel off the real Haxial TextEdit Find window) or the
// theme's Primary / Text Box / Button / Default Button color groups. Same
// primitive-base + optional color-overlay contract as the rest of the kit.
#pragma once
#include "canvas.h"
#include "chrome.h"

struct DialogColors {
    Color workspace; // panel background
    Color label;     // static label text
    Color field_bg, field_fg, field_frame, field_focus, caret;
    Color btn_l2, btn_l1, btn, btn_d1, btn_d2, btn_frame, btn_label;
    Color def_light, def_button, def_frame; // default-button ring
};

inline DialogColors dialog_colors(const Theme *theme) {
    if (!theme || !theme->has_colors) {
        // KDX Standard, measured off the real Find and Replace window:
        // dark-grey panel, dark-grey buttons with white labels, black
        // fields with a #880000 focus ring and #CC0000 caret.
        return {Color{51, 51, 51},    kWhite,
                kBlack,               Color{0, 204, 0},
                Color{17, 17, 17},    Color{136, 0, 0},
                Color{204, 0, 0},     Color{102, 102, 102},
                Color{68, 68, 68},    Color{51, 51, 51},
                Color{34, 34, 34},    Color{17, 17, 17},
                kBlack,               kWhite,
                Color{204, 0, 0},     Color{136, 0, 0},
                kBlack};
    }
    auto c = [&](int i) { return from_u32(theme->color(i)); };
    return {c(ColPrimaryBackground),
            c(ColPrimaryLabel),
            c(ColTextBoxBackground),
            c(ColTextBoxForeground),
            c(ColPrimaryFrame),
            c(ColFocusBox),
            c(ColTextInsertionPoint),
            c(ColButtonLight2),
            c(ColButtonLight2 + 1),
            c(ColButtonLight2 + 2),
            c(ColButtonLight2 + 3),
            c(ColButtonLight2 + 4),
            c(ColButtonFrame),
            c(ColButtonLabel),
            c(ColDefaultButtonLight),
            c(ColDefaultButtonLight + 1),
            c(ColDefaultButtonLight + 3)};
}

// A sunken text field: black box with a thin frame (a 2px Focus Box
// outline when active) and a Text Insertion Point caret after the contents.
inline void draw_field(Canvas &cv, Rect r, const char *text, bool focused,
                        bool caret_on, const DialogColors &dc) {
    cv.fill(r, dc.field_bg);
    if (focused) {
        cv.frame(r, dc.field_focus);
        cv.frame({r.x + 1, r.y + 1, r.w - 2, r.h - 2}, dc.field_focus);
    } else {
        cv.frame(r, dc.field_frame);
    }
    int tx = r.x + 5;
    int ty = r.y + (r.h - kFontHeight) / 2;
    int end = cv.text(tx, ty, text, dc.field_fg);
    if (focused && caret_on)
        cv.vline(end + 1, r.y + 3, r.bottom() - 3, dc.caret);
}

// A frame with the 4 corner pixels cut to `bg`, giving Haxial's slightly
// rounded button/edge look.
inline void rounded_frame(Canvas &cv, Rect r, Color frame, Color bg) {
    cv.hline(r.x + 1, r.right() - 1, r.y, frame);
    cv.hline(r.x + 1, r.right() - 1, r.bottom() - 1, frame);
    cv.vline(r.x, r.y + 1, r.bottom() - 1, frame);
    cv.vline(r.right() - 1, r.y + 1, r.bottom() - 1, frame);
    cv.put(r.x, r.y, pack(bg));
    cv.put(r.right() - 1, r.y, pack(bg));
    cv.put(r.x, r.bottom() - 1, pack(bg));
    cv.put(r.right() - 1, r.bottom() - 1, pack(bg));
}

// A push button as the real window draws it: rounded black frame, 2px
// Light2/Light1 bevel top-left, 2px Dark1/Dark2 shadow bottom-right, dark
// face, centred label. Depresses when pressed. The default button gets the
// Default Button ring (black / Light / Button) around it.
inline void draw_button(Canvas &cv, Rect r, const char *label, bool pressed,
                        bool is_default, const DialogColors &dc) {
    if (is_default) {
        rounded_frame(cv, r, dc.def_frame, dc.workspace);
        cv.frame({r.x + 1, r.y + 1, r.w - 2, r.h - 2}, dc.def_light);
        cv.frame({r.x + 2, r.y + 2, r.w - 4, r.h - 4}, dc.def_button);
        r = {r.x + 3, r.y + 3, r.w - 6, r.h - 6};
    }
    cv.fill(r, dc.btn);
    rounded_frame(cv, r, dc.btn_frame, dc.workspace);
    Color l2 = pressed ? dc.btn_d2 : dc.btn_l2;
    Color l1 = pressed ? dc.btn_d1 : dc.btn_l1;
    Color d1 = pressed ? dc.btn_l1 : dc.btn_d1;
    Color d2 = pressed ? dc.btn_l2 : dc.btn_d2;
    cv.hline(r.x + 1, r.right() - 1, r.y + 1, l2);
    cv.hline(r.x + 2, r.right() - 2, r.y + 2, l1);
    cv.vline(r.x + 1, r.y + 1, r.bottom() - 1, l2);
    cv.vline(r.x + 2, r.y + 2, r.bottom() - 2, l1);
    cv.hline(r.x + 2, r.right() - 2, r.bottom() - 3, d1);
    cv.hline(r.x + 1, r.right() - 1, r.bottom() - 2, d2);
    cv.vline(r.right() - 3, r.y + 2, r.bottom() - 2, d1);
    cv.vline(r.right() - 2, r.y + 1, r.bottom() - 1, d2);
    int tw = cv.text_width(label);
    int off = pressed ? 1 : 0;
    cv.text(r.x + (r.w - tw) / 2 + off, r.y + (r.h - kFontHeight) / 2 + off,
            label, dc.btn_label);
}

// A checkbox: rounded black frame, light top-left bevel, dark face (like a
// small flat button), with a tick when set. Label follows.
inline void draw_checkbox(Canvas &cv, int x, int y, bool checked,
                          const char *label, const DialogColors &dc) {
    Rect b{x, y, 14, 14};
    cv.fill(b, dc.btn);
    rounded_frame(cv, b, dc.btn_frame, dc.workspace);
    cv.hline(b.x + 1, b.right() - 1, b.y + 1, dc.btn_l2);
    cv.vline(b.x + 1, b.y + 1, b.bottom() - 1, dc.btn_l2);
    cv.hline(b.x + 1, b.right() - 1, b.bottom() - 2, dc.btn_d2);
    cv.vline(b.right() - 2, b.y + 1, b.bottom() - 1, dc.btn_d2);
    if (checked) {
        // A thick tick mark.
        for (int i = 0; i < 4; ++i) {
            cv.put(b.x + 3, b.y + 6 + i, pack(dc.btn_label));
            cv.put(b.x + 4, b.y + 7 + i, pack(dc.btn_label));
        }
        for (int i = 0; i < 6; ++i) {
            cv.put(b.x + 5 + i, b.y + 9 - i, pack(dc.btn_label));
            cv.put(b.x + 5 + i, b.y + 10 - i, pack(dc.btn_label));
        }
    }
    cv.text(x + 20, y + (14 - kFontHeight) / 2, label, dc.label);
}
