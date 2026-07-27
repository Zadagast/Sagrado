// Sagrado Kit dialog controls: text fields, checkboxes and push buttons,
// drawn into the framebuffer from the KDX Standard primitives or the theme's
// Button / Text Box / Workspace color groups. Same primitive-base + optional
// color-overlay contract as the rest of the kit.
#pragma once
#include "canvas.h"
#include "chrome.h"

struct DialogColors {
    Color workspace;                  // panel background
    Color label;                      // static label text
    Color field_bg, field_fg, field_frame, field_focus; // text fields
    Color btn_l2, btn_l1, btn, btn_d1, btn_d2, btn_frame, btn_label;
    Color def_light, def_button, def_dark, def_frame; // default-button ring
};

inline DialogColors dialog_colors(const Theme *theme) {
    if (!theme || !theme->has_colors) {
        // Standard: the platinum-grey control look Haxial ships with.
        Color g2{238, 238, 238}, g1{212, 208, 200}, gb{212, 208, 200},
            d1{128, 128, 128}, d2{64, 64, 64};
        return {Color{136, 0, 0}, kWhite,
                kBlack,           Color{0, 204, 0},
                kDeep,            kBright,
                g2,               g1,
                gb,               d1,
                d2,               kBlack,
                kBlack,           kWhite,
                Color{212, 208, 200}, Color{64, 64, 64},
                kBright};
    }
    auto c = [&](int i) { return from_u32(theme->color(i)); };
    return {c(ColWorkspaceBackground1),
            c(ColPrimaryLabel),
            c(ColTextBoxBackground),
            c(ColTextBoxForeground),
            c(ColPrimaryFrame),
            c(ColFocusBox),
            c(ColButtonLight2),
            c(ColButtonLight2 + 1),
            c(ColButtonLight2 + 2),
            c(ColButtonLight2 + 3),
            c(ColButtonLight2 + 4),
            c(ColButtonFrame),
            c(ColButtonLabel),
            c(ColDefaultButtonLight),
            c(ColDefaultButtonLight + 1),
            c(ColDefaultButtonLight + 2),
            c(ColDefaultButtonLight + 3)};
}

// A sunken text field: black box with a thin frame (a red focus outline
// when active), and a red insertion caret after the contents.
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
        cv.vline(end + 1, r.y + 3, r.bottom() - 3, dc.field_focus);
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

// A raised push button with a centred label and slightly rounded corners;
// the default button gets an extra rounded ring, and it depresses when
// pressed.
inline void draw_button(Canvas &cv, Rect r, const char *label, bool pressed,
                        bool is_default, const DialogColors &dc) {
    if (is_default) {
        rounded_frame(cv, r, dc.def_frame, dc.workspace);
        rounded_frame(cv, {r.x + 1, r.y + 1, r.w - 2, r.h - 2}, dc.def_frame,
                      dc.workspace);
        r = {r.x + 3, r.y + 3, r.w - 6, r.h - 6};
    }
    cv.fill(r, dc.btn);
    rounded_frame(cv, r, dc.btn_frame, dc.workspace);
    Color tl = pressed ? dc.btn_d2 : dc.btn_l2;
    Color br = pressed ? dc.btn_l2 : dc.btn_d2;
    cv.hline(r.x + 2, r.right() - 2, r.y + 1, tl);
    cv.vline(r.x + 1, r.y + 2, r.bottom() - 2, tl);
    cv.hline(r.x + 2, r.right() - 2, r.bottom() - 2, br);
    cv.vline(r.right() - 2, r.y + 2, r.bottom() - 2, br);
    int tw = cv.text_width(label);
    int off = pressed ? 1 : 0;
    cv.text(r.x + (r.w - tw) / 2 + off, r.y + (r.h - kFontHeight) / 2 + off,
            label, dc.btn_label);
}

// A checkbox (small sunken box, tick when set) followed by its label.
inline void draw_checkbox(Canvas &cv, int x, int y, bool checked,
                          const char *label, const DialogColors &dc) {
    Rect b{x, y, 13, 13};
    cv.fill(b, dc.field_bg);
    cv.frame(b, dc.btn_frame);
    // Sunken: dark shadow top/left, light highlight bottom/right.
    cv.hline(b.x + 1, b.right() - 1, b.y + 1, dc.btn_d2);
    cv.vline(b.x + 1, b.y + 1, b.bottom() - 1, dc.btn_d2);
    cv.hline(b.x + 1, b.right() - 1, b.bottom() - 1, dc.btn_d1);
    cv.vline(b.right() - 1, b.y + 1, b.bottom() - 1, dc.btn_d1);
    if (checked) {
        // A thick tick mark.
        for (int i = 0; i < 4; ++i) {
            cv.put(b.x + 3, b.y + 5 + i, pack(dc.field_fg));
            cv.put(b.x + 4, b.y + 6 + i, pack(dc.field_fg));
        }
        for (int i = 0; i < 6; ++i) {
            cv.put(b.x + 5 + i, b.y + 8 - i, pack(dc.field_fg));
            cv.put(b.x + 5 + i, b.y + 9 - i, pack(dc.field_fg));
        }
    }
    cv.text(x + 19, y + (13 - kFontHeight) / 2, label, dc.label);
}
