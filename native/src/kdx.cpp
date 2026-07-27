// Sagrado KDX — the P2P client. Native Win32, drawn entirely into a
// software framebuffer on the shared Sagrado Kit (same chrome, controls and
// .hap theming as Sagrado TextEdit), modeled on the real Haxial KDX main
// window: a narrow launcher with a stack of command buttons.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#include <string>
#include <vector>

#include "canvas.h"
#include "chrome.h"
#include "controls.h"

namespace {

constexpr int kWinW = 336, kWinH = 590;

const char *kButtons[] = {
    "Commands",  "Connect...",     "Address Book", "Messages", 
    "File Transfers", "File Browser", "Chat",     "News",
    "User List", "Exit",
};
constexpr int kButtonCount = 10;

struct App {
    Canvas canvas;
    ChromeLayout lay{};
    bool focused = true;
    int pressed_box = 0;
    int hot_box = 0;
    int pressed_btn = -1; // launcher button being pressed
    Rect btns[kButtonCount]{};
    Rect logo{};
    Theme theme;
    bool themed = false;
} g_app;

HWND g_main = nullptr;
HINSTANCE g_hinst = nullptr;

const Theme *active_theme() { return g_app.themed ? &g_app.theme : nullptr; }

// A launcher row: a kit button with a small icon well and a left-aligned
// label, like the real KDX main window.
void draw_launcher_button(Canvas &cv, Rect r, const char *label, bool pressed,
                          const DialogColors &dc) {
    cv.fill(r, dc.btn);
    rounded_frame(cv, r, dc.btn_frame, dc.workspace);
    Color l2 = pressed ? dc.btn_d2 : dc.btn_l2;
    Color d2 = pressed ? dc.btn_l2 : dc.btn_d2;
    cv.hline(r.x + 1, r.right() - 1, r.y + 1, l2);
    cv.vline(r.x + 1, r.y + 1, r.bottom() - 1, l2);
    cv.hline(r.x + 1, r.right() - 1, r.bottom() - 2, d2);
    cv.vline(r.right() - 2, r.y + 1, r.bottom() - 1, d2);
    int off = pressed ? 1 : 0;
    // Icon well (placeholder glyph until we have per-command icons).
    Rect ic{r.x + 6 + off, r.y + (r.h - 12) / 2 + off, 12, 12};
    cv.frame(ic, dc.btn_d2);
    cv.fill({ic.x + 3, ic.y + 3, 6, 6}, dc.def_light);
    cv.text(r.x + 26 + off, r.y + (r.h - kFontHeight) / 2 + off, label,
            dc.btn_label);
}

// The KDX medallion plate (placeholder art: framed plate + wordmark).
void draw_logo(Canvas &cv, Rect r, const DialogColors &dc) {
    cv.fill(r, kBlack);
    rounded_frame(cv, r, dc.btn_frame, dc.workspace);
    cv.hline(r.x + 1, r.right() - 1, r.y + 1, dc.btn_l1);
    cv.vline(r.x + 1, r.y + 1, r.bottom() - 1, dc.btn_l1);
    // Concentric medallion rings.
    int cxm = r.x + r.w / 2, cym = r.y + r.h / 2;
    for (int ring = 0; ring < 3; ++ring) {
        int rw = r.w / 2 - 14 - ring * 12;
        int rh = r.h / 2 - 8 - ring * 8;
        if (rw < 20 || rh < 12) break;
        cv.frame({cxm - rw, cym - rh, rw * 2, rh * 2},
                 ring == 1 ? dc.def_button : dc.btn_d1);
    }
    // "KDX" wordmark, drawn 3x from the small font.
    const char *word = "KDX";
    int tw = cv.text_width(word) * 3;
    int x0 = cxm - tw / 2, y0 = cym - (kFontHeight * 3) / 2;
    Canvas tmp;
    tmp.resize(cv.text_width(word) + 2, kFontHeight + 2);
    tmp.fill({0, 0, tmp.width(), tmp.height()}, kBlack);
    tmp.text(0, 0, word, kWhite);
    for (int y = 0; y < tmp.height(); ++y)
        for (int x = 0; x < tmp.width(); ++x)
            if (tmp.get(x, y) != pack(kBlack))
                cv.fill({x0 + x * 3, y0 + y * 3, 3, 3}, dc.label);
}

void repaint(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right, h = rc.bottom;
    if (w <= 0 || h <= 0) return;
    Canvas &cv = g_app.canvas;
    if (cv.width() != w || cv.height() != h) cv.resize(w, h);
    const Theme *theme = active_theme();
    g_app.lay = chrome_layout(w, h, theme, g_app.focused);
    g_app.lay.grip = {0, 0, 0, 0}; // fixed-size launcher: no grow box

    paint_chrome(cv, g_app.lay, "KDX", g_app.focused, g_app.hot_box,
                 g_app.pressed_box, theme);

    DialogColors dc = dialog_colors(theme);
    Rect cl = g_app.lay.client;
    cv.fill(cl, dc.workspace);

    // Logo plate.
    g_app.logo = {cl.x + 4, cl.y + 4, cl.w - 8, 128};
    draw_logo(cv, g_app.logo, dc);

    // The command button stack.
    int y = g_app.logo.bottom() + 8;
    int bh = 26, gap = 4;
    for (int i = 0; i < kButtonCount; ++i) {
        g_app.btns[i] = {cl.x + 6, y, cl.w - 12, bh};
        draw_launcher_button(cv, g_app.btns[i], kButtons[i],
                             g_app.pressed_btn == i, dc);
        y += bh + gap;
    }

    // Connection counters (users / transfers), like the real footer.
    int fy = cl.bottom() - kFontHeight - 6;
    cv.fill({cl.x + 10, fy + 2, 8, 8}, Color{64, 128, 255});
    cv.text(cl.x + 24, fy, "0", dc.label);
    cv.fill({cl.x + 60, fy + 2, 8, 8}, Color{204, 0, 0});
    cv.text(cl.x + 74, fy, "0", dc.label);
}

void blit_canvas(HDC hdc, const Canvas &cv) {
    if (cv.width() == 0) return;
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = cv.width();
    bmi.bmiHeader.biHeight = -cv.height(); // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    SetDIBitsToDevice(hdc, 0, 0, cv.width(), cv.height(), 0, 0, 0,
                      cv.height(), cv.data(), &bmi, DIB_RGB_COLORS);
}

int button_at(int x, int y) {
    for (int i = 0; i < kButtonCount; ++i)
        if (g_app.btns[i].contains(x, y)) return i;
    return -1;
}

void run_command(int i, HWND hwnd) {
    const char *name = kButtons[i];
    if (lstrcmpA(name, "Exit") == 0) {
        DestroyWindow(hwnd);
        return;
    }
    // Remaining commands come online one at a time; Connect... is next.
    char msg[128];
    wsprintfA(msg, "\"%s\" is not wired up yet.", name);
    MessageBoxA(hwnd, msg, "Sagrado KDX", MB_OK);
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_NCCALCSIZE:
            if (wp) return 0;
            break;
        case WM_NCHITTEST:
            return HTCLIENT;
        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            if (g_app.lay.close_box.contains(x, y)) {
                g_app.pressed_box = 1;
                SetCapture(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (g_app.lay.min_box.contains(x, y)) {
                CloseWindow(hwnd); // minimize
                return 0;
            }
            int b = button_at(x, y);
            if (b >= 0) {
                g_app.pressed_btn = b;
                SetCapture(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (y < g_app.lay.title_h)
                SendMessage(hwnd, WM_SYSCOMMAND, SC_MOVE + 2, lp);
            return 0;
        }
        case WM_LBUTTONUP: {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            if (g_app.pressed_box == 1) {
                ReleaseCapture();
                g_app.pressed_box = 0;
                if (g_app.lay.close_box.contains(x, y)) {
                    DestroyWindow(hwnd);
                    return 0;
                }
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (g_app.pressed_btn >= 0) {
                ReleaseCapture();
                int was = g_app.pressed_btn;
                g_app.pressed_btn = -1;
                InvalidateRect(hwnd, nullptr, FALSE);
                if (button_at(x, y) == was) run_command(was, hwnd);
                return 0;
            }
            return 0;
        }
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) DestroyWindow(hwnd);
            return 0;
        case WM_TIMER: {
            bool f = GetForegroundWindow() == hwnd;
            if (f != g_app.focused) {
                g_app.focused = f;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_ACTIVATE:
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            repaint(hwnd);
            blit_canvas(hdc, g_app.canvas);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

} // namespace

int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int show) {
    g_hinst = hinst;

    WNDCLASSA wc{};
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = hinst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "SagradoKDX";
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(0, "SagradoKDX", "KDX",
                                WS_POPUP | WS_MINIMIZEBOX, CW_USEDEFAULT,
                                CW_USEDEFAULT, kWinW, kWinH, nullptr, nullptr,
                                hinst, nullptr);
    g_main = hwnd;
    ShowWindow(hwnd, show);
    SetTimer(hwnd, 1, 250, nullptr);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
