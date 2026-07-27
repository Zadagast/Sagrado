// A Sagrado Kit pop-up menu: a borderless top-level window drawn entirely in
// the framebuffer, so it looks the same everywhere and skins with the .hap
// menu colors. Used by the KDX launcher's command buttons, which drop their
// menu beside the button the way the real client does.
#pragma once
#include <windows.h>

#include <string>
#include <vector>

#include "canvas.h"
#include "chrome.h"

namespace menu {

// A 16x16-ish icon in the menu's left column. Pixels equal to the menu
// background are treated as transparent, so icons cut from program art sit
// correctly on the hilite bar.
struct Icon {
    int w = 0, h = 0;
    const uint32_t *px = nullptr;
};

// id 0 marks a separator row.
struct Item {
    std::string label;
    int id = 0;
    bool enabled = true;
    Icon icon{};
    std::string shortcut; // right-aligned, e.g. "^K"
    bool submenu = false; // draws the right-pointing arrow
};

inline Item separator() { return {"", 0, false, {}, "", false}; }

using Handler = void (*)(int id);
using Notify = void (*)();

// Metrics measured off the real KDX Commands menu: 18px rows (separators take
// a full row with an engraved line through it), a 16px icon column inset 5px
// from the frame, labels at 26px, and a 12px right margin.
constexpr int kItemH = 18;
constexpr int kSepH = 18;
constexpr int kIconX = 5;
constexpr int kIconW = 16;
constexpr int kLabelX = 26;
constexpr int kRightPad = 12;
constexpr int kGapMin = 24; // between a label and its shortcut

struct Popup {
    HWND hwnd = nullptr;
    HWND owner = nullptr;
    Canvas canvas;
    std::vector<Item> items;
    Handler on_select = nullptr;
    Notify on_close = nullptr;
    int hot = -1;
    bool armed = false; // set once the opening click has been released
};

inline Popup g_menu;

inline bool is_open() { return g_menu.hwnd != nullptr; }

inline int item_top(int index) {
    int y = 2;
    for (int i = 0; i < index; ++i)
        y += g_menu.items[i].id == 0 ? kSepH : kItemH;
    return y;
}

inline int item_at(int y) {
    for (size_t i = 0; i < g_menu.items.size(); ++i) {
        int top = item_top(int(i));
        int h = g_menu.items[i].id == 0 ? kSepH : kItemH;
        if (y >= top && y < top + h)
            return g_menu.items[i].id == 0 || !g_menu.items[i].enabled
                       ? -1
                       : int(i);
    }
    return -1;
}

inline void close() {
    if (!g_menu.hwnd) return;
    HWND h = g_menu.hwnd;
    g_menu.hwnd = nullptr;
    ReleaseCapture();
    DestroyWindow(h);
    if (g_menu.on_close) g_menu.on_close();
}

inline void paint(HDC hdc) {
    RECT rc;
    GetClientRect(g_menu.hwnd, &rc);
    int w = rc.right, h = rc.bottom;
    Canvas &cv = g_menu.canvas;
    if (cv.width() != w || cv.height() != h) cv.resize(w, h);
    UiColors uc = ui_colors(nullptr);

    Rect r{0, 0, w, h};
    cv.fill(r, uc.bar_body);
    cv.frame(r, kBlack);
    cv.hline(1, w - 1, 1, uc.bar_light);
    cv.vline(1, 1, h - 1, uc.bar_light);
    cv.hline(1, w - 1, h - 2, uc.bar_dark);
    cv.vline(w - 2, 1, h - 1, uc.bar_dark);

    for (size_t i = 0; i < g_menu.items.size(); ++i) {
        const Item &it = g_menu.items[i];
        int top = item_top(int(i));
        if (it.id == 0) { // separator: an engraved line across the panel
            cv.hline(1, w - 1, top + 7, uc.bar_dark);
            cv.hline(1, w - 1, top + 8, uc.bar_light);
            continue;
        }
        Rect row{2, top, w - 4, kItemH};
        bool hot = int(i) == g_menu.hot;
        if (hot) cv.fill(row, uc.hilite);
        Color fg = !it.enabled ? uc.bar_light : hot ? uc.hilite_text : uc.text;
        if (it.icon.px) {
            uint32_t bg = pack(uc.bar_body);
            int iy = top + (kItemH - it.icon.h) / 2;
            for (int y = 0; y < it.icon.h; ++y)
                for (int x = 0; x < it.icon.w; ++x) {
                    uint32_t p = it.icon.px[size_t(y) * it.icon.w + x];
                    if (p != bg) cv.put(kIconX + x, iy + y, p);
                }
        }
        cv.text(kLabelX, row.y + 1, it.label.c_str(), fg);
        if (!it.shortcut.empty()) {
            int sw = cv.text_width(it.shortcut.c_str());
            cv.text(w - kRightPad - sw, row.y + 1, it.shortcut.c_str(), fg);
        }
        if (it.submenu) { // right-pointing triangle, as in the real menu
            int ax = w - kRightPad - 4, ay = row.y + kItemH / 2;
            for (int k = 0; k < 5; ++k)
                cv.vline(ax - 4 + k, ay - 4 + k, ay + 5 - k, fg);
        }
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = cv.width();
    bmi.bmiHeader.biHeight = -cv.height();
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    SetDIBitsToDevice(hdc, 0, 0, cv.width(), cv.height(), 0, 0, 0, cv.height(),
                      cv.data(), &bmi, DIB_RGB_COLORS);
}

inline LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE; // keep the owner window looking active
        case WM_NCHITTEST:
            return HTCLIENT;
        case WM_MOUSEMOVE: {
            POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            RECT rc;
            GetClientRect(hwnd, &rc);
            int hot = (p.x >= 0 && p.x < rc.right) ? item_at(p.y) : -1;
            if (hot != g_menu.hot) {
                g_menu.hot = hot;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            // Press-and-drag opens then picks on release; a plain click leaves
            // the menu up until the next click, like the real client.
            if (g_menu.hot >= 0) {
                int id = g_menu.items[g_menu.hot].id;
                Handler h = g_menu.on_select;
                close();
                if (h) h(id);
            } else {
                g_menu.armed = true;
            }
            return 0;
        }
        case WM_LBUTTONDOWN: {
            if (!g_menu.armed) return 0;
            if (g_menu.hot >= 0) {
                int id = g_menu.items[g_menu.hot].id;
                Handler h = g_menu.on_select;
                close();
                if (h) h(id);
            } else {
                close();
            }
            return 0;
        }
        case WM_RBUTTONDOWN:
        case WM_CANCELMODE:
        case WM_KILLFOCUS:
            close();
            return 0;
        case WM_TIMER:
            // Clicking another application takes the capture away (and does
            // not always send WM_CANCELMODE under Wine): drop the menu.
            if (GetCapture() != hwnd || GetForegroundWindow() != g_menu.owner)
                close();
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (g_menu.hwnd == hwnd) paint(hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

// Drop a menu with its top-left corner at (sx, sy) in screen coordinates,
// nudged back on screen if it would fall off an edge.
inline void open(HINSTANCE hinst, HWND owner, int sx, int sy,
                 std::vector<Item> items, Handler on_select,
                 Notify on_close = nullptr) {
    close();
    static bool registered = false;
    if (!registered) {
        WNDCLASSA wc{};
        wc.lpfnWndProc = wnd_proc;
        wc.hInstance = hinst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = "SagradoMenu";
        RegisterClassA(&wc);
        registered = true;
    }
    g_menu.items = std::move(items);
    g_menu.on_select = on_select;
    g_menu.on_close = on_close;
    g_menu.owner = owner;
    g_menu.hot = -1;
    g_menu.armed = false;

    Canvas measure;
    int wmax = 0, h = 4;
    for (const Item &it : g_menu.items) {
        if (it.id == 0) {
            h += kSepH;
            continue;
        }
        h += kItemH;
        int tw = measure.text_width(it.label.c_str());
        if (!it.shortcut.empty())
            tw += kGapMin + measure.text_width(it.shortcut.c_str());
        if (it.submenu) tw += kGapMin;
        if (tw > wmax) wmax = tw;
    }
    int w = kLabelX + wmax + kRightPad;

    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    if (sx + w > screen_w) sx = screen_w - w;
    if (sy + h > screen_h) sy = screen_h - h;
    if (sx < 0) sx = 0;
    if (sy < 0) sy = 0;

    g_menu.hwnd = CreateWindowExA(WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
                                  "SagradoMenu", "", WS_POPUP, sx, sy, w, h,
                                  owner, nullptr, hinst, nullptr);
    ShowWindow(g_menu.hwnd, SW_SHOWNOACTIVATE);
    SetCapture(g_menu.hwnd);
    SetTimer(g_menu.hwnd, 1, 100, nullptr);
}

} // namespace menu
