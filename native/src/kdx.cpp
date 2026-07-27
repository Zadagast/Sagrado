// Sagrado KDX — the P2P client. Native Win32, drawn entirely into a
// software framebuffer on the shared Sagrado Kit (same chrome, controls and
// .hap theming as Sagrado TextEdit). The main window reproduces the real
// Haxial KDX launcher at its exact 164x310 metrics, with the medallion and
// command icons extracted pixel-for-pixel from the original (in real KDX
// these are program art, not appearance art).
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#include <string>
#include <vector>

#include "canvas.h"
#include "chrome.h"
#include "controls.h"
#include "kdx_art.h"

namespace {

constexpr int kWinW = 164, kWinH = 310;

// The KDX butterfly glyph drawn beside the title (12x11, from the real
// window's title bar).
constexpr int kLogoW = 12, kLogoH = 11;
constexpr uint16_t kLogoGlyph[] = {0x0f0f, 0x0891, 0x0861, 0x0841,
                                   0x0881, 0x0f0f, 0x0811, 0x0821,
                                   0x0861, 0x0891, 0x0f0f};

struct Command {
    const char *label;
    const ArtImage *icon;
    int top, bottom; // row bounds measured from the real window
};

const Command kCommands[] = {
    {"Commands", &kIcCommands, 66, 93},
    {"Connect...", &kIcConnect, 93, 114},
    {"Address Book", &kIcAddress, 114, 135},
    {"Messages", &kIcMessages, 135, 156},
    {"File Transfers", &kIcTransfers, 156, 177},
    {"File Browser", &kIcBrowser, 177, 198},
    {"Chat", &kIcChat, 198, 219},
    {"News", &kIcNews, 219, 240},
    {"User List", &kIcUsers, 240, 261},
    {"Exit", &kIcExit, 261, 282},
};
constexpr int kCommandCount = 10;

struct App {
    Canvas canvas;
    ChromeLayout lay{};
    bool focused = true;
    int pressed_box = 0;
    int hot_box = 0;
    int pressed_btn = -1;
    int connections = 0, transfers = 0;
} g_app;

HWND g_main = nullptr;
HINSTANCE g_hinst = nullptr;

void blit_art(Canvas &cv, const ArtImage &a, int dx = 0, int dy = 0) {
    for (int y = 0; y < a.h; ++y)
        for (int x = 0; x < a.w; ++x)
            cv.put(a.ox + x + dx, a.oy + y + dy, a.px[size_t(y) * a.w + x]);
}

Rect command_rect(int i) {
    return {10, kCommands[i].top, 144, kCommands[i].bottom - kCommands[i].top + 1};
}

// A launcher row at the real KDX metrics: shared 1px black borders, double
// light bevel top/left, double shadow bevel bottom/right, #333 face.
void draw_command(Canvas &cv, int i, bool pressed, const DialogColors &dc) {
    Rect r = command_rect(i);
    cv.frame(r, dc.btn_frame);
    Color l1 = pressed ? dc.btn_d1 : dc.btn_l1;
    Color l2 = pressed ? dc.btn_d2 : dc.btn_l2;
    Color d1 = pressed ? dc.btn_l1 : dc.btn_d1;
    Color d2 = pressed ? dc.btn_l2 : dc.btn_d2;
    cv.hline(r.x + 1, r.right() - 1, r.y + 1, l1);
    cv.vline(r.x + 1, r.y + 1, r.bottom() - 1, l1);
    cv.hline(r.x + 2, r.right() - 2, r.y + 2, l2);
    cv.vline(r.x + 2, r.y + 2, r.bottom() - 2, l2);
    cv.hline(r.x + 2, r.right() - 1, r.bottom() - 2, d1);
    cv.vline(r.right() - 2, r.y + 2, r.bottom() - 1, d1);
    cv.hline(r.x + 3, r.right() - 2, r.bottom() - 3, d2);
    cv.vline(r.right() - 3, r.y + 3, r.bottom() - 2, d2);
    cv.fill({r.x + 3, r.y + 3, r.w - 6, r.h - 6}, dc.btn);
    int off = pressed ? 1 : 0;
    blit_art(cv, *kCommands[i].icon, off, off);
    int ty = r.y + (r.h - kFontHeight) / 2 + off;
    cv.text(36 + off, ty, kCommands[i].label, dc.btn_label);
    if (kCommands[i].icon == &kIcMessages) blit_art(cv, kWonderLight, off, off);
}

void repaint(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right, h = rc.bottom;
    if (w <= 0 || h <= 0) return;
    Canvas &cv = g_app.canvas;
    if (cv.width() != w || cv.height() != h) cv.resize(w, h);
    g_app.lay = chrome_layout(w, h, nullptr, g_app.focused);
    g_app.lay.grip = {0, 0, 0, 0};      // fixed-size launcher: no grow box
    g_app.lay.close_box = {0, 0, 0, 0}; // real KDX main window shows only
    g_app.lay.hatch_box = {0, 0, 0, 0}; // the minimize box; Exit quits
    g_app.lay.max_box = {0, 0, 0, 0};

    paint_chrome(cv, g_app.lay, "", g_app.focused, g_app.hot_box,
                 g_app.pressed_box, nullptr);

    // Centered butterfly glyph + "KDX" title, as in the real window.
    {
        const char *title = "KDX";
        int tw = kLogoW + 6 + cv.text_width(title);
        int tx = (w - tw) / 2;
        Color tc = g_app.focused ? kWhite : Color{204, 204, 204};
        for (int y = 0; y < kLogoH; ++y)
            for (int x = 0; x < kLogoW; ++x)
                if (kLogoGlyph[y] & (1 << x)) cv.put(tx + x, 5 + y, pack(tc));
        cv.text(tx + kLogoW + 6, 4, title, tc);
    }

    DialogColors dc = dialog_colors(nullptr);
    cv.fill(g_app.lay.client, dc.workspace);

    blit_art(cv, kMedallion);

    for (int i = 0; i < kCommandCount; ++i)
        draw_command(cv, i, g_app.pressed_btn == i, dc);

    // Connection / transfer counters, as in the real footer.
    blit_art(cv, kFootUser1);
    blit_art(cv, kFootUser2);
    char buf[16];
    int ty = 286 + (16 - kFontHeight) / 2;
    wsprintfA(buf, "%d", g_app.connections);
    cv.text(49, ty, buf, dc.label);
    wsprintfA(buf, "%d", g_app.transfers);
    cv.text(107, ty, buf, dc.label);
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
    for (int i = 0; i < kCommandCount; ++i)
        if (command_rect(i).contains(x, y)) return i;
    return -1;
}

void run_command(int i, HWND hwnd) {
    const char *name = kCommands[i].label;
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
            if (g_app.lay.min_box.contains(x, y)) {
                g_app.pressed_box = 4;
                SetCapture(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
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
            if (g_app.pressed_box == 4) {
                ReleaseCapture();
                g_app.pressed_box = 0;
                InvalidateRect(hwnd, nullptr, FALSE);
                if (g_app.lay.min_box.contains(x, y)) CloseWindow(hwnd);
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
