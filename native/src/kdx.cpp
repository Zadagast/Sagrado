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

// --- Tracker window ------------------------------------------------------
// Modeled on the real KDX tracker: a groups table on top, the selected
// group's server list below. Sample data until the Cloudflare tracker
// backend is wired in.

struct TrackerGroup {
    const char *name;
};

struct TrackerServer {
    const char *group;
    const char *name;
    int users;
    const char *date;
    const char *desc;
};

const TrackerGroup kGroups[] = {
    {"Business"}, {"Chat"},     {"Education"}, {"Games"},
    {"General"},  {"Macintosh"}, {"Trackers"},  {"Windows"},
};
constexpr int kGroupCount = 8;

const TrackerServer kServers[] = {
    {"General", "Loophole's Lair", 5, "26/07/08 09:26 PM", ""},
    {"General", "Skynet", 3, "26/05/05 10:17 AM", "..."},
    {"General", "higher intellect", 7, "26/05/05 10:17 AM",
     "750,000+ text files :: old/rare software archive"},
    {"General", "Faceless Server", 2, "26/07/12 04:02 PM",
     "[Storage for the afterworld]  Private German Server"},
    {"General", "Inverted Reality", 10, "26/05/05 10:17 AM",
     "kdx.inverted.be"},
    {"General", "stickytack", 6, "26/07/14 12:27 PM",
     "kdx.stickytack.com  |  The best server on KDX since before 2004!"},
    {"Chat", "The Lobby", 12, "26/07/20 08:00 PM", "Come hang out."},
    {"Chat", "Night Owls", 4, "26/07/18 02:11 AM", "Late night chat."},
    {"Games", "Retro Arcade", 3, "26/07/01 05:39 PM",
     "Abandonware and high scores."},
    {"Macintosh", "Major Mac Backup", 1, "26/05/05 10:17 AM",
     "Your Archiving Resource"},
};
constexpr int kServerCount = int(sizeof(kServers) / sizeof(kServers[0]));

struct TrackerWnd {
    HWND hwnd = nullptr;
    Canvas canvas;
    bool focused = true;
    int pressed_box = 0;
    int sel_group = 4; // General
    ChromeLayout lay{};
    Rect group_rows[kGroupCount]{};
} g_tracker;

constexpr int kTrkW = 860, kTrkH = 560;
constexpr int kRowH = 18, kHdrH = 17, kSbW = 13;

void draw_list_header(Canvas &cv, Rect r, const char *cols[], const int w[],
                      int n) {
    // White-to-grey vertical gradient with dark labels, like the real one.
    for (int y = 0; y < r.h; ++y) {
        int v = 255 - (y * 60) / r.h;
        cv.hline(r.x, r.right(), r.y + y, Color{uint8_t(v), uint8_t(v),
                                                uint8_t(v)});
    }
    cv.frame(r, kBlack);
    int x = r.x;
    for (int i = 0; i < n; ++i) {
        cv.text(x + 6, r.y + (r.h - kFontHeight) / 2 + 1, cols[i],
                Color{51, 51, 51});
        x += w[i];
        if (i + 1 < n) cv.vline(x, r.y + 1, r.bottom() - 1, Color{136, 136, 136});
    }
}

// A dead-simple KDX-style scrollbar gutter (inert until lists overflow).
void draw_scroll_gutter(Canvas &cv, Rect r, bool vertical) {
    cv.fill(r, Color{51, 51, 51});
    cv.frame(r, kBlack);
    Color a{136, 136, 136};
    if (vertical) {
        cv.fill({r.x + 3, r.y + 4, r.w - 6, 2}, a);
        cv.fill({r.x + 3, r.y + 8, r.w - 6, 2}, a);
        cv.fill({r.x + 3, r.bottom() - 6, r.w - 6, 2}, a);
        cv.fill({r.x + 3, r.bottom() - 10, r.w - 6, 2}, a);
    } else {
        cv.fill({r.x + 4, r.y + 3, 2, r.h - 6}, a);
        cv.fill({r.x + 8, r.y + 3, 2, r.h - 6}, a);
        cv.fill({r.right() - 6, r.y + 3, 2, r.h - 6}, a);
        cv.fill({r.right() - 10, r.y + 3, 2, r.h - 6}, a);
    }
}

int servers_in_group(const char *g) {
    int n = 0;
    for (int i = 0; i < kServerCount; ++i)
        if (lstrcmpA(kServers[i].group, g) == 0) ++n;
    return n;
}

void paint_tracker() {
    Canvas &cv = g_tracker.canvas;
    RECT rc;
    GetClientRect(g_tracker.hwnd, &rc);
    int w = rc.right, h = rc.bottom;
    if (w <= 0 || h <= 0) return;
    if (cv.width() != w || cv.height() != h) cv.resize(w, h);
    bool focused = GetForegroundWindow() == g_tracker.hwnd;
    g_tracker.focused = focused;

    ChromeLayout lay = chrome_layout(w, h, nullptr, focused);
    lay.grip = {0, 0, 0, 0};
    lay.max_box = {0, 0, 0, 0};
    g_tracker.lay = lay;
    paint_chrome(cv, lay, "Tracker: Sagrado Tracker", focused, 0,
                 g_tracker.pressed_box == 5 ? 1 : 0, nullptr);

    Rect cl = lay.client;
    cv.fill(cl, Color{51, 51, 51});

    Color row_a{68, 68, 68}, row_b{51, 51, 51};
    Color sel_fill{102, 0, 0}, sel_frame{136, 0, 0};

    // --- Groups pane -----------------------------------------------------
    int top_h = kHdrH + kGroupCount * kRowH + kRowH * 2 + kSbW;
    Rect gp{cl.x + 2, cl.y + 2, cl.w - 4, top_h};
    const char *gcols[] = {"Count", "Group Name", "Description"};
    int gw[] = {56, 200, gp.w - kSbW - 256};
    draw_list_header(cv, {gp.x, gp.y, gp.w - kSbW, kHdrH}, gcols, gw, 3);
    int y = gp.y + kHdrH;
    for (int i = 0; i < kGroupCount; ++i) {
        Rect row{gp.x, y, gp.w - kSbW, kRowH};
        g_tracker.group_rows[i] = row;
        bool sel = i == g_tracker.sel_group;
        cv.fill(row, sel ? sel_fill : (i % 2 ? row_b : row_a));
        if (sel) cv.frame(row, sel_frame);
        char cnt[16];
        wsprintfA(cnt, "%d", servers_in_group(kGroups[i].name));
        int cw = cv.text_width(cnt);
        int ty = y + (kRowH - kFontHeight) / 2 + 1;
        cv.text(row.x + gw[0] - 8 - cw, ty, cnt, kWhite);
        cv.text(row.x + gw[0] + 6, ty, kGroups[i].name, kWhite);
        y += kRowH;
    }
    draw_scroll_gutter(cv, {gp.right() - kSbW, gp.y, kSbW, gp.h - kSbW}, true);
    draw_scroll_gutter(cv, {gp.x, gp.bottom() - kSbW, gp.w - kSbW, kSbW},
                       false);

    // --- Servers pane ------------------------------------------------------
    Rect sp{cl.x + 2, gp.bottom() + 6, cl.w - 4,
            cl.bottom() - gp.bottom() - 8};
    const char *scols[] = {"Server Name", "Users", "Date Online",
                           "Server Description"};
    int sw[] = {200, 52, 150, sp.w - kSbW - 402};
    draw_list_header(cv, {sp.x, sp.y, sp.w - kSbW, kHdrH}, scols, sw, 4);
    y = sp.y + kHdrH;
    int row_i = 0;
    const char *sel_name = kGroups[g_tracker.sel_group].name;
    for (int i = 0; i < kServerCount && y + kRowH <= sp.bottom() - kSbW; ++i) {
        if (lstrcmpA(kServers[i].group, sel_name) != 0) continue;
        Rect row{sp.x, y, sp.w - kSbW, kRowH};
        cv.fill(row, row_i % 2 ? row_b : row_a);
        int ty = y + (kRowH - kFontHeight) / 2 + 1;
        cv.text(row.x + 6, ty, kServers[i].name, kWhite);
        char cnt[16];
        wsprintfA(cnt, "%d", kServers[i].users);
        cv.text(row.x + sw[0] + sw[1] - 8 - cv.text_width(cnt), ty, cnt,
                kWhite);
        cv.text(row.x + sw[0] + sw[1] + 6, ty, kServers[i].date, kWhite);
        cv.text(row.x + sw[0] + sw[1] + sw[2] + 6, ty, kServers[i].desc,
                kWhite);
        y += kRowH;
        ++row_i;
    }
    draw_scroll_gutter(cv, {sp.right() - kSbW, sp.y, kSbW, sp.h - kSbW},
                       true);
    draw_scroll_gutter(cv, {sp.x, sp.bottom() - kSbW, sp.w - kSbW, kSbW},
                       false);
}

void blit_canvas(HDC hdc, const Canvas &cv);

LRESULT CALLBACK tracker_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_NCCALCSIZE:
            if (wp) return 0;
            break;
        case WM_NCHITTEST:
            return HTCLIENT;
        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            if (g_tracker.lay.close_box.contains(x, y)) {
                g_tracker.pressed_box = 5;
                SetCapture(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (g_tracker.lay.min_box.contains(x, y)) {
                CloseWindow(hwnd);
                return 0;
            }
            for (int i = 0; i < kGroupCount; ++i) {
                if (g_tracker.group_rows[i].contains(x, y)) {
                    g_tracker.sel_group = i;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
            }
            if (y < g_tracker.lay.title_h)
                SendMessage(hwnd, WM_SYSCOMMAND, SC_MOVE + 2, lp);
            return 0;
        }
        case WM_LBUTTONUP:
            if (g_tracker.pressed_box == 5) {
                ReleaseCapture();
                g_tracker.pressed_box = 0;
                if (g_tracker.lay.close_box.contains(GET_X_LPARAM(lp),
                                                     GET_Y_LPARAM(lp))) {
                    DestroyWindow(hwnd);
                    return 0;
                }
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) DestroyWindow(hwnd);
            return 0;
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
            paint_tracker();
            blit_canvas(hdc, g_tracker.canvas);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            g_tracker.hwnd = nullptr;
            if (g_main) SetForegroundWindow(g_main);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

void open_tracker(HINSTANCE hinst) {
    if (g_tracker.hwnd) {
        SetForegroundWindow(g_tracker.hwnd);
        return;
    }
    static bool registered = false;
    if (!registered) {
        WNDCLASSA wc{};
        wc.lpfnWndProc = tracker_proc;
        wc.hInstance = hinst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = "SagradoTracker";
        RegisterClassA(&wc);
        registered = true;
    }
    RECT mr{};
    if (g_main) GetWindowRect(g_main, &mr);
    g_tracker.hwnd = CreateWindowExA(0, "SagradoTracker", "Tracker", WS_POPUP,
                                     mr.right + 12, mr.top, kTrkW, kTrkH,
                                     g_main, nullptr, hinst, nullptr);
    ShowWindow(g_tracker.hwnd, SW_SHOW);
    SetForegroundWindow(g_tracker.hwnd);
}

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
    if (lstrcmpA(name, "Connect...") == 0) {
        open_tracker(g_hinst);
        return;
    }
    // Remaining commands come online one at a time.
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
