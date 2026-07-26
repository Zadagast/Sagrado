// Sagrado TextEdit — native Win32, drawn entirely into a software
// framebuffer and blitted with SetDIBitsToDevice, the way Haxial built KDX.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#include "canvas.h"
#include "chrome.h"

namespace {

struct App {
    Canvas canvas;
    ChromeLayout lay{};
    bool focused = true;
    int pressed_box = 0; // 1 close, 3 max, 4 min
    int hot_box = 0;
} g_app;

constexpr int kMenuH = 20;
constexpr int kTabH = 26;

const char *kMenus[] = {"File", "Tools", "Favorites", "Location", "Appearance"};

void paint_content(Canvas &cv, const ChromeLayout &lay) {
    Rect c = lay.client;

    // Menu bar.
    Rect menu{c.x, c.y, c.w, kMenuH};
    raised_bar(cv, menu);
    int x = menu.x + 12;
    for (const char *m : kMenus) {
        x = cv.text(x, menu.y + 2, m, kWhite);
        x += 18;
    }
    // Save-state indicator triangle at the right of the menu bar.
    int tx = menu.right() - 16;
    for (int i = 0; i < 6; ++i)
        cv.hline(tx - i, tx + i + 1, menu.y + 4 + i, kGlyphGrey);

    // Tab strip with one active red tab.
    Rect strip{c.x, menu.bottom(), c.w, kTabH};
    raised_bar(cv, strip);
    Rect tab{strip.x + 9, strip.y + 4, 78, kTabH - 4};
    bevel_box(cv, tab, false);
    cv.fill({tab.x + 6, tab.y + 5, 8, 8}, kWhite);
    cv.frame({tab.x + 6, tab.y + 5, 8, 8}, kDeep);
    cv.text(tab.x + 20, tab.y + 3, "Untitled", kWhite);

    // Editor area: black, with sample green text.
    Rect editor{c.x, strip.bottom(), c.w - kScrollbar, c.bottom() - strip.bottom()};
    cv.fill({c.x, strip.bottom(), c.w, c.bottom() - strip.bottom()}, kBlack);
    const char *lines[] = {
        "'Twas brillig, and the slithy toves",
        "Did gyre and gimble in the wabe;",
        "All mimsy were the borogoves,",
        "And the mome raths outgrabe.",
    };
    int y = editor.y + 4;
    for (const char *ln : lines) {
        cv.text(editor.x + 4, y, ln, Color{0, 204, 0});
        y += kFontHeight + 1;
    }

    // Vertical scrollbar, stopping above the grow box.
    Rect sb{c.right() - kScrollbar + 1, editor.y, kScrollbar,
            g_app.lay.grip.y - editor.y};
    cv.fill(sb, kTrack);
    cv.frame(sb, kBlack);
    // Arrow boxes: a dec+inc pair at each end.
    auto arrow_box = [&](int y0, int h, bool up) {
        Rect b{sb.x, y0, sb.w, h};
        cv.fill(b, kBarBody);
        cv.frame(b, kBlack);
        cv.hline(b.x + 1, b.right() - 1, b.y + 1, kThumbHi);
        cv.vline(b.x + 1, b.y + 1, b.bottom() - 1, kThumbHi);
        int cx = b.x + b.w / 2, cy = b.y + b.h / 2;
        for (int i = 0; i < 4; ++i) {
            int w = up ? i : 3 - i;
            cv.hline(cx - w, cx + w + 1, cy - 2 + i, kGlyphGrey);
        }
    };
    int ah = 13;
    arrow_box(sb.y, ah, true);
    arrow_box(sb.y + ah - 1, ah, false);
    arrow_box(sb.bottom() - 2 * ah + 1, ah, true);
    arrow_box(sb.bottom() - ah, ah, false);
    // Thumb.
    Rect thumb{sb.x, sb.y + 2 * ah - 1, sb.w, 40};
    cv.fill(thumb, kThumb);
    cv.frame(thumb, kBlack);
    cv.hline(thumb.x + 1, thumb.right() - 1, thumb.y + 1, kThumbHi);
    cv.vline(thumb.x + 1, thumb.y + 1, thumb.bottom() - 1, kThumbHi);
    for (int i = 0; i < 3; ++i)
        cv.hline(thumb.x + 4, thumb.right() - 4,
                 thumb.y + thumb.h / 2 - 3 + i * 3, kBarDark);
}

void repaint(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right, h = rc.bottom;
    if (w <= 0 || h <= 0) return;
    Canvas &cv = g_app.canvas;
    if (cv.width() != w || cv.height() != h) cv.resize(w, h);
    g_app.lay = chrome_layout(w, h);

    paint_chrome(cv, g_app.lay, "TE: Untitled", g_app.focused, g_app.hot_box,
                 g_app.pressed_box);
    paint_content(cv, g_app.lay);
    paint_grip(cv, g_app.lay.grip);
}

void blit(HDC hdc) {
    const Canvas &cv = g_app.canvas;
    if (cv.width() == 0) return;
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = cv.width();
    bmi.bmiHeader.biHeight = -cv.height(); // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    SetDIBitsToDevice(hdc, 0, 0, cv.width(), cv.height(), 0, 0, 0, cv.height(),
                      cv.data(), &bmi, DIB_RGB_COLORS);
}

int box_at(int x, int y) {
    if (g_app.lay.close_box.contains(x, y)) return 1;
    if (g_app.lay.max_box.contains(x, y)) return 3;
    if (g_app.lay.min_box.contains(x, y)) return 4;
    return 0;
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_NCCALCSIZE:
            // Claim the whole window as client area: we draw the frame.
            if (wp) return 0;
            break;
        case WM_NCHITTEST: {
            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ScreenToClient(hwnd, &pt);
            int x = pt.x, y = pt.y;
            const ChromeLayout &lay = g_app.lay;
            if (box_at(x, y)) return HTCLIENT;
            if (lay.grip.contains(x, y)) return HTBOTTOMRIGHT;
            int w = lay.window.w, h = lay.window.h;
            bool left = x < kBorder, right = x >= w - kBorder;
            bool top = y < 3, bottom = y >= h - kBorder;
            if (top && left) return HTTOPLEFT;
            if (top && right) return HTTOPRIGHT;
            if (bottom && left) return HTBOTTOMLEFT;
            if (bottom && right) return HTBOTTOMRIGHT;
            if (top) return HTTOP;
            if (bottom) return HTBOTTOM;
            if (left) return HTLEFT;
            if (right) return HTRIGHT;
            if (y < kTitleH) return HTCAPTION;
            return HTCLIENT;
        }
        case WM_LBUTTONDOWN: {
            int b = box_at(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            if (b) {
                g_app.pressed_box = b;
                SetCapture(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            if (g_app.pressed_box) {
                ReleaseCapture();
                int b = box_at(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
                int was = g_app.pressed_box;
                g_app.pressed_box = 0;
                InvalidateRect(hwnd, nullptr, FALSE);
                if (b == was) {
                    if (b == 1) DestroyWindow(hwnd);
                    if (b == 3)
                        ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
                    if (b == 4) ShowWindow(hwnd, SW_MINIMIZE);
                }
            }
            return 0;
        }
        case WM_ACTIVATE:
            g_app.focused = LOWORD(wp) != WA_INACTIVE;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_SIZE:
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            repaint(hwnd);
            blit(hdc);
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
    WNDCLASSA wc{};
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = hinst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "SagradoWindow";
    RegisterClassA(&wc);

    // WS_THICKFRAME + WM_NCCALCSIZE=0 gives native move/resize/minimize with
    // an entirely self-drawn frame.
    HWND hwnd = CreateWindowExA(
        0, "SagradoWindow", "Sagrado TextEdit",
        WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 760, 520, nullptr, nullptr, hinst,
        nullptr);
    ShowWindow(hwnd, show);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
