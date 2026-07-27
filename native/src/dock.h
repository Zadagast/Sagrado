// The Haxial Dock — where windows go when minimized.
//
// Official Haxial behaviour (Generic Documentation, "Anatomy of a Window"):
// left-click Minimize hides the window and adds a button to a shared
// "{App} Dock" window; clicking that button restores it. Double-clicking a
// window's title bar does the same as Minimize. This is not OS iconify
// (CloseWindow / SW_MINIMIZE) and not the macOS Dock.
//
// The Dock itself uses ordinary kit chrome. There is no dedicated .hap Dock
// slot; buttons use the Button colour group (and Push Button art when we
// wire slots later). Icons on the buttons are program art per window type.
#pragma once

#include <windows.h>
#include <windowsx.h>

#include <string>
#include <vector>

#include "canvas.h"
#include "chrome.h"
#include "controls.h"

namespace dock {

struct Icon {
    int w = 0, h = 0;
    const uint32_t *px = nullptr;
};

struct Entry {
    HWND hwnd = nullptr;
    std::string title;
    Icon icon{};
};

struct Window {
    HWND hwnd = nullptr;
    Canvas canvas;
    ChromeLayout lay{};
    int pressed_box = 0;  // ChromeClose while held
    int pressed_item = -1;
    TitleDrag title_drag{};
    std::vector<Entry> items;
};

inline Window g;

constexpr int kDockW = 220;
constexpr int kItemH = 22;
constexpr int kItemPad = 6;
constexpr int kIconSlot = 20;

inline int client_h_for(int n) {
    if (n < 1) n = 1;
    return kItemPad * 2 + n * kItemH + (n - 1) * 2;
}

inline int item_top(int i) { return kItemPad + i * (kItemH + 2); }

inline Rect item_rect(const ChromeLayout &lay, int i) {
    return {lay.client.x + kItemPad, lay.client.y + item_top(i),
            lay.client.w - 2 * kItemPad, kItemH};
}

inline void blit_icon(Canvas &cv, const Icon &ic, int x, int y) {
    if (!ic.px || ic.w <= 0 || ic.h <= 0) return;
    for (int iy = 0; iy < ic.h; ++iy)
        for (int ix = 0; ix < ic.w; ++ix) {
            uint32_t p = ic.px[size_t(iy) * ic.w + ix];
            if ((p & 0xffffff) != 0x333333) cv.put(x + ix, y + iy, p);
        }
}

inline void layout_size() {
    if (!g.hwnd) return;
    int ch = client_h_for(int(g.items.size()));
    // Outer size: client plus Standard borders; chrome_layout will refine
    // when themed frame art is present.
    int w = kDockW, h = kTitleH + kBorder + ch;
    const Theme *theme = kit_theme();
    ChromeLayout probe = chrome_layout(w, h, theme, true);
    chrome_dialog_boxes(probe);
    probe.min_box = {0, 0, 0, 0};
    probe.grip = {0, 0, 0, 0};
    // Grow height so the client matches our button stack.
    int need = probe.client.y + ch + (h - probe.client.bottom());
    if (need < h) need = h;
    SetWindowPos(g.hwnd, HWND_TOPMOST, 0, 0, w, need,
                 SWP_NOMOVE | SWP_NOACTIVATE);
}

inline void paint() {
    if (!g.hwnd) return;
    RECT rc;
    GetClientRect(g.hwnd, &rc);
    int w = rc.right, h = rc.bottom;
    if (w <= 0 || h <= 0) return;
    if (g.canvas.width() != w || g.canvas.height() != h) g.canvas.resize(w, h);
    bool focused = GetForegroundWindow() == g.hwnd;
    const Theme *theme = kit_theme();
    g.lay = chrome_layout(w, h, theme, focused);
    chrome_dialog_boxes(g.lay);
    g.lay.min_box = {0, 0, 0, 0};
    g.lay.grip = {0, 0, 0, 0};

    paint_chrome(g.canvas, g.lay, "KDX Dock", focused, 0, g.pressed_box, theme);
    DialogColors dc = dialog_colors(theme);
    g.canvas.fill(g.lay.client, dc.workspace);

    for (size_t i = 0; i < g.items.size(); ++i) {
        Rect r = item_rect(g.lay, int(i));
        bool pressed = int(i) == g.pressed_item;
        draw_button(g.canvas, r, "", pressed, false, dc);
        const Entry &e = g.items[i];
        int ix = r.x + 4 + (pressed ? 1 : 0);
        int iy = r.y + (r.h - (e.icon.h > 0 ? e.icon.h : 14)) / 2 +
                 (pressed ? 1 : 0);
        blit_icon(g.canvas, e.icon, ix, iy);
        int tx = r.x + kIconSlot + (pressed ? 1 : 0);
        g.canvas.set_clip({tx, r.y, r.right() - tx - 4, r.h});
        g.canvas.text(tx, r.y + (r.h - kFontHeight) / 2 + (pressed ? 1 : 0),
                      e.title.c_str(), dc.btn_label);
        g.canvas.clear_clip();
    }
}

inline void blit(HDC hdc) {
    if (g.canvas.width() == 0) return;
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = g.canvas.width();
    bmi.bmiHeader.biHeight = -g.canvas.height();
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    SetDIBitsToDevice(hdc, 0, 0, g.canvas.width(), g.canvas.height(), 0, 0, 0,
                      g.canvas.height(), g.canvas.data(), &bmi, DIB_RGB_COLORS);
}

inline int item_at(int x, int y) {
    for (size_t i = 0; i < g.items.size(); ++i)
        if (item_rect(g.lay, int(i)).contains(x, y)) return int(i);
    return -1;
}

inline void restore_at(int index) {
    if (index < 0 || index >= int(g.items.size())) return;
    HWND target = g.items[index].hwnd;
    g.items.erase(g.items.begin() + index);
    if (target) {
        ShowWindow(target, SW_SHOW);
        SetForegroundWindow(target);
    }
    if (g.items.empty()) {
        if (g.hwnd) {
            HWND h = g.hwnd;
            g.hwnd = nullptr;
            DestroyWindow(h);
        }
        return;
    }
    layout_size();
    InvalidateRect(g.hwnd, nullptr, FALSE);
}

inline void restore_hwnd(HWND target) {
    for (size_t i = 0; i < g.items.size(); ++i) {
        if (g.items[i].hwnd == target) {
            restore_at(int(i));
            return;
        }
    }
    if (target) {
        ShowWindow(target, SW_SHOW);
        SetForegroundWindow(target);
    }
}

inline void restore_all() {
    std::vector<HWND> hs;
    for (const Entry &e : g.items) hs.push_back(e.hwnd);
    g.items.clear();
    for (HWND h : hs)
        if (h) {
            ShowWindow(h, SW_SHOW);
            SetForegroundWindow(h);
        }
    if (g.hwnd) {
        HWND d = g.hwnd;
        g.hwnd = nullptr;
        DestroyWindow(d);
    }
}

inline void forget(HWND target) {
    if (!target) return;
    for (size_t i = 0; i < g.items.size(); ++i) {
        if (g.items[i].hwnd != target) continue;
        g.items.erase(g.items.begin() + i);
        if (g.items.empty()) {
            if (g.hwnd) {
                HWND d = g.hwnd;
                g.hwnd = nullptr;
                DestroyWindow(d);
            }
        } else if (g.hwnd) {
            layout_size();
            InvalidateRect(g.hwnd, nullptr, FALSE);
        }
        return;
    }
}

inline bool is_docked(HWND target) {
    for (const Entry &e : g.items)
        if (e.hwnd == target) return true;
    return false;
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

inline void ensure(HINSTANCE hinst) {
    if (g.hwnd) return;
    static bool registered = false;
    if (!registered) {
        WNDCLASSA wc{};
        wc.lpfnWndProc = wnd_proc;
        wc.hInstance = hinst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = "SagradoDock";
        RegisterClassA(&wc);
        registered = true;
    }
    int ch = client_h_for(1);
    int w = kDockW, h = kTitleH + kBorder + ch;
    // Park near the bottom-left of the work area, like a tray companion.
    RECT wa{};
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &wa, 0);
    int x = wa.left + 12;
    int y = wa.bottom - h - 12;
    g.hwnd = CreateWindowExA(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, "SagradoDock",
                             "KDX Dock", WS_POPUP, x, y, w, h, nullptr,
                             nullptr, hinst, nullptr);
    ShowWindow(g.hwnd, SW_SHOWNOACTIVATE);
    SetWindowPos(g.hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

inline void minimize(HWND target, const char *title, Icon icon,
                     HINSTANCE hinst) {
    if (!target || target == g.hwnd) return;
    if (is_docked(target)) return;
    ensure(hinst);
    Entry e;
    e.hwnd = target;
    e.title = title && title[0] ? title : "Window";
    e.icon = icon;
    g.items.push_back(std::move(e));
    ShowWindow(target, SW_HIDE);
    layout_size();
    if (g.hwnd) {
        ShowWindow(g.hwnd, SW_SHOWNOACTIVATE);
        SetWindowPos(g.hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        InvalidateRect(g.hwnd, nullptr, FALSE);
        UpdateWindow(g.hwnd);
    }
}

inline LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_NCCALCSIZE:
            if (wp) return 0;
            break;
        case WM_NCHITTEST:
            return HTCLIENT;
        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            int box = chrome_box_at(g.lay, x, y);
            if (box == ChromeClose) {
                g.pressed_box = ChromeClose;
                SetCapture(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            int item = item_at(x, y);
            if (item >= 0) {
                g.pressed_item = item;
                SetCapture(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (y < g.lay.title_h) {
                g.title_drag.arm(x, y);
                SetCapture(hwnd);
            }
            return 0;
        }
        case WM_MOUSEMOVE:
            if (g.title_drag.armed)
                g.title_drag.maybe_drag(hwnd, GET_X_LPARAM(lp),
                                        GET_Y_LPARAM(lp), lp);
            return 0;
        case WM_LBUTTONUP: {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            g.title_drag.clear();
            if (g.pressed_box == ChromeClose) {
                ReleaseCapture();
                g.pressed_box = 0;
                if (chrome_box_at(g.lay, x, y) == ChromeClose) {
                    restore_all();
                    return 0;
                }
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (g.pressed_item >= 0) {
                ReleaseCapture();
                int was = g.pressed_item;
                g.pressed_item = -1;
                if (item_at(x, y) == was) {
                    restore_at(was);
                    return 0;
                }
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            paint();
            blit(hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            if (g.hwnd == hwnd) g.hwnd = nullptr;
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

} // namespace dock
