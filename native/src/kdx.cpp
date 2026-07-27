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
#include "host_room.h"
#include "tracker.h"

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
// group's server list below. The rows come from the Sagrado tracker (a
// Cloudflare Worker); see tracker.h.

using tracker::kGroupNames;
constexpr int kGroupCount = tracker::kGroupCount;

constexpr int kTrkW = 900, kTrkH = 600;
constexpr int kPaneInset = 8;  // window margin + the pane's focus ring
constexpr int kRowH = 18, kHdrH = 18, kSbW = 13, kArrowLen = 15;

// Measured off the real KDX tracker window.
const Color kListBg{68, 68, 68};     // List Background
const Color kSortBg{51, 51, 51};     // Sort Column Background
const Color kRowLine{102, 102, 102}; // 1px separator above each row
const Color kSelFill{102, 0, 0};     // selected row band
const Color kPlate{51, 51, 51};      // header / scrollbar plate face
const Color kPlateHi{102, 102, 102};
const Color kPlateLo{34, 34, 34};
const Color kGlyph{136, 136, 136};   // scrollbar arrows
const Color kFocusBorder{136, 0, 0}; // focused pane ring
const Color kIdleBorder{17, 17, 17};

// The raised plate used by header cells, scroll arrows and the thumb.
inline void plate(Canvas &cv, Rect b) {
    cv.fill(b, kPlate);
    cv.frame(b, kBlack);
    cv.hline(b.x + 1, b.right() - 1, b.y + 1, kPlateHi);
    cv.vline(b.x + 1, b.y + 1, b.bottom() - 1, kPlateHi);
    cv.hline(b.x + 1, b.right() - 1, b.bottom() - 2, kPlateLo);
    cv.vline(b.right() - 2, b.y + 1, b.bottom() - 1, kPlateLo);
}

// A KDX scrollbar: an arrow pair at each end, a draggable thumb between.
struct ScrollBar {
    Rect r{};
    bool vertical = true;
    int value = 0, page = 1, total = 1;
    Rect dec_a{}, inc_a{}, dec_b{}, inc_b{}, track{}, thumb{};

    int max_value() const {
        int m = total - page;
        return m > 0 ? m : 0;
    }
    void set(int v) {
        int m = max_value();
        value = v < 0 ? 0 : (v > m ? m : v);
    }

    void layout() {
        set(value);
        int len = vertical ? r.h : r.w;
        int a = kArrowLen;
        if (len < a * 5) a = len / 5;
        auto box = [&](int off, int size) {
            return vertical ? Rect{r.x, r.y + off, r.w, size}
                            : Rect{r.x + off, r.y, size, r.h};
        };
        dec_a = box(0, a);
        inc_a = box(a, a);
        dec_b = box(len - 2 * a, a);
        inc_b = box(len - a, a);
        int t0 = 2 * a, tlen = len - 4 * a;
        if (tlen < 0) tlen = 0;
        track = box(t0, tlen);
        int span = vertical ? track.h : track.w;
        int th = total > 0 ? span * page / total : span;
        if (th < 16) th = 16;
        if (th > span) th = span;
        int pos = max_value() > 0 ? (span - th) * value / max_value() : 0;
        thumb = box(t0 + pos, th);
    }

    void paint(Canvas &cv) const {
        cv.fill(r, kPlate);
        cv.frame(r, kBlack);
        auto arrow = [&](Rect b, bool back) {
            plate(cv, b);
            int cx = b.x + b.w / 2, cy = b.y + b.h / 2;
            for (int i = 0; i < 4; ++i) {
                int t = back ? i : 3 - i;
                if (vertical)
                    cv.hline(cx - t, cx + t + 1, cy - 2 + i, kGlyph);
                else
                    cv.vline(cx - 2 + i, cy - t, cy + t + 1, kGlyph);
            }
        };
        arrow(dec_a, true);
        arrow(inc_a, false);
        arrow(dec_b, true);
        arrow(inc_b, false);
        plate(cv, thumb);
        // Grip: four short lines at the thumb's center.
        int cx = thumb.x + thumb.w / 2, cy = thumb.y + thumb.h / 2;
        for (int i = 0; i < 4; ++i) {
            if (vertical && thumb.h > 24)
                cv.hline(cx - 4, cx + 4, cy - 4 + i * 2, kPlateHi);
            else if (!vertical && thumb.w > 24)
                cv.vline(cx - 4 + i * 2, cy - 4, cy + 4, kPlateHi);
        }
    }

    // Handle a press; returns 1 for a step/page change, 2 when the thumb
    // was grabbed (caller starts a drag), 0 when the press missed.
    int on_press(int x, int y) {
        if (dec_a.contains(x, y) || dec_b.contains(x, y)) {
            set(value - (vertical ? 1 : 16));
            return 1;
        }
        if (inc_a.contains(x, y) || inc_b.contains(x, y)) {
            set(value + (vertical ? 1 : 16));
            return 1;
        }
        if (thumb.contains(x, y)) return 2;
        if (track.contains(x, y)) {
            bool before = vertical ? y < thumb.y : x < thumb.x;
            set(value + (before ? -page : page));
            return 1;
        }
        return 0;
    }

    // Move the thumb so its leading edge sits at `pos` (client coords).
    void drag_to(int pos) {
        int span = (vertical ? track.h : track.w) - (vertical ? thumb.h : thumb.w);
        int rel = pos - (vertical ? track.y : track.x);
        set(span > 0 ? rel * max_value() / span : 0);
    }
};

struct Column {
    const char *title;
    int w;
    bool right_align;
};

// A KDX list: header, column-shaded body (the sorted column is tinted),
// and both scrollbars.
struct ListPane {
    Rect r{};
    Rect header{}, body{};
    ScrollBar vsb, hsb;
    int sort_col = 0;

    void layout(const Column *cols, int ncols, int rows) {
        int total_w = 0;
        for (int i = 0; i < ncols; ++i) total_w += cols[i].w;
        header = {r.x, r.y, r.w - kSbW, kHdrH};
        body = {r.x, r.y + kHdrH, r.w - kSbW, r.h - kHdrH - kSbW};
        vsb.vertical = true;
        vsb.r = {r.right() - kSbW, r.y, kSbW, r.h - kSbW};
        vsb.page = body.h / kRowH;
        vsb.total = rows;
        hsb.vertical = false;
        hsb.r = {r.x, r.bottom() - kSbW, r.w - kSbW, kSbW};
        hsb.page = body.w;
        hsb.total = total_w;
        vsb.layout();
        hsb.layout();
    }

    int row_at(int y) const {
        if (y < body.y || y >= body.bottom()) return -1;
        return vsb.value + (y - body.y) / kRowH;
    }
    Rect row_rect(int index) const {
        return {body.x, body.y + (index - vsb.value) * kRowH, body.w, kRowH};
    }
    int col_x(const Column *cols, int i) const {
        int x = body.x - hsb.value;
        for (int k = 0; k < i; ++k) x += cols[k].w;
        return x;
    }
};

void paint_pane(Canvas &cv, ListPane &p, const Column *cols, int ncols,
                bool focused) {
    // Focus ring: red around the active pane, near-black otherwise.
    Rect ring{p.r.x - 3, p.r.y - 3, p.r.w + 6, p.r.h + 6};
    Color rc = focused ? kFocusBorder : kIdleBorder;
    cv.fill({ring.x, ring.y, ring.w, 2}, rc);
    cv.fill({ring.x, ring.bottom() - 2, ring.w, 2}, rc);
    cv.fill({ring.x, ring.y, 2, ring.h}, rc);
    cv.fill({ring.right() - 2, ring.y, 2, ring.h}, rc);
    cv.frame({ring.x + 2, ring.y + 2, ring.w - 4, ring.h - 4}, kBlack);

    // Header: a raised plate per column, white labels.
    cv.set_clip(p.header);
    cv.fill(p.header, kPlate);
    int x = p.header.x - p.hsb.value;
    for (int i = 0; i < ncols; ++i) {
        plate(cv, {x, p.header.y, cols[i].w + 1, p.header.h});
        cv.text(x + 6, p.header.y + (p.header.h - kFontHeight) / 2 + 1,
                cols[i].title, kWhite);
        x += cols[i].w;
    }
    cv.clear_clip();

    // Body: list background, the sorted column tinted, row separators.
    cv.set_clip(p.body);
    cv.fill(p.body, kListBg);
    x = p.body.x - p.hsb.value;
    for (int i = 0; i < ncols; ++i) {
        if (i == p.sort_col)
            cv.fill({x, p.body.y, cols[i].w, p.body.h}, kSortBg);
        x += cols[i].w;
    }
    for (int y = p.body.y; y < p.body.bottom(); y += kRowH)
        cv.hline(p.body.x, p.body.right(), y, kRowLine);
    cv.clear_clip();

    p.vsb.paint(cv);
    p.hsb.paint(cv);
}

void draw_cell(Canvas &cv, int x, int w, int y, const char *text,
               bool right_align) {
    int tx = right_align ? x + w - 8 - cv.text_width(text) : x + 6;
    cv.text(tx, y, text, kWhite);
}

const Column kGroupCols[] = {
    {"Count", 56, true}, {"Group Name", 200, false}, {"Description", 620, false}};
constexpr int kGroupColCount = 3;

const Column kServerCols[] = {{"Server Name", 200, false},
                              {"Users", 60, true},
                              {"Date Online", 150, false},
                              {"Server Description", 700, false}};
constexpr int kServerColCount = 4;

// A scoped CRITICAL_SECTION guard for the fetched directory.
struct Lock {
    CRITICAL_SECTION &cs;
    explicit Lock(CRITICAL_SECTION &c) : cs(c) { EnterCriticalSection(&cs); }
    ~Lock() { LeaveCriticalSection(&cs); }
};

constexpr UINT WM_TRACKER = WM_APP + 1;  // a fetch finished

struct TrackerWnd {
    HWND hwnd = nullptr;
    tracker::Directory dir, pending;
    CRITICAL_SECTION mutex{};
    Canvas canvas;
    bool focused = true;
    int pressed_box = 0;
    int sel_group = 4;  // General
    int sel_server = -1;
    int focus_pane = 0;
    ChromeLayout lay{};
    ListPane groups, servers;
    ScrollBar *drag = nullptr;
    int drag_grab = 0;
} g_tracker;

int servers_in_group(const char *g) {
    int n = 0;
    for (const tracker::Room &r : g_tracker.dir.rooms)
        if (r.group == g) ++n;
    return n;
}

// Indices of the rooms listed under the selected group.
std::vector<int> group_server_indices() {
    std::vector<int> out;
    const char *g = kGroupNames[g_tracker.sel_group];
    for (size_t i = 0; i < g_tracker.dir.rooms.size(); ++i)
        if (g_tracker.dir.rooms[i].group == g) out.push_back(int(i));
    return out;
}

// One directory fetch, off the UI thread; the window redraws on WM_TRACKER.
DWORD WINAPI fetch_thread(LPVOID) {
    std::string body;
    tracker::Directory dir;
    if (net::request(tracker::base_url() + "/rooms", "", body) &&
        tracker::parse(body, dir)) {
        dir.status = tracker::Ready;
    } else {
        dir.status = tracker::Failed;
        dir.error = "Could not reach " + tracker::base_url();
    }
    {
        Lock lock(g_tracker.mutex);
        g_tracker.pending = dir;
    }
    if (g_tracker.hwnd) PostMessage(g_tracker.hwnd, WM_TRACKER, 0, 0);
    return 0;
}

void refresh_tracker() {
    if (g_tracker.dir.status == tracker::Fetching) return;
    g_tracker.dir.status = tracker::Fetching;
    if (HANDLE t = CreateThread(nullptr, 0, fetch_thread, nullptr, 0, nullptr))
        CloseHandle(t);
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
    lay.max_box = {0, 0, 0, 0};
    g_tracker.lay = lay;
    paint_chrome(cv, lay, "Tracker: Sagrado Tracker", focused, 0,
                 g_tracker.pressed_box == 5 ? 1 : 0, nullptr);

    Rect cl = lay.client;
    cv.fill(cl, Color{51, 51, 51});

    ListPane &gp = g_tracker.groups;
    ListPane &sp = g_tracker.servers;
    int top_h = kHdrH + 10 * kRowH + kSbW;
    if (top_h > cl.h / 2) top_h = cl.h / 2;
    gp.r = {cl.x + kPaneInset, cl.y + kPaneInset, cl.w - 2 * kPaneInset,
            top_h};
    gp.sort_col = 1;  // sorted by Group Name
    gp.layout(kGroupCols, kGroupColCount, kGroupCount);
    paint_pane(cv, gp, kGroupCols, kGroupColCount,
               focused && g_tracker.focus_pane == 0);

    cv.set_clip(gp.body);
    for (int i = gp.vsb.value;
         i < kGroupCount && gp.row_rect(i).y < gp.body.bottom(); ++i) {
        Rect row = gp.row_rect(i);
        if (i == g_tracker.sel_group)
            cv.fill({row.x, row.y + 1, row.w, row.h - 1}, kSelFill);
        int ty = row.y + (kRowH - kFontHeight) / 2 + 1;
        char cnt[16];
        wsprintfA(cnt, "%d", servers_in_group(kGroupNames[i]));
        draw_cell(cv, gp.col_x(kGroupCols, 0), kGroupCols[0].w, ty, cnt, true);
        draw_cell(cv, gp.col_x(kGroupCols, 1), kGroupCols[1].w, ty,
                  kGroupNames[i], false);
    }
    cv.clear_clip();

    std::vector<int> idx = group_server_indices();
    int n = int(idx.size());
    // The pane runs to the same margin all round; the grow box, painted
    // last, notches its bottom-right corner exactly like the real tracker.
    sp.r = {cl.x + kPaneInset, gp.r.bottom() + 9, cl.w - 2 * kPaneInset,
            cl.bottom() - kPaneInset - gp.r.bottom() - 9};
    if (sp.r.h < kHdrH + kRowH + kSbW) sp.r.h = kHdrH + kRowH + kSbW;
    sp.sort_col = 3;  // sorted by Server Description
    sp.layout(kServerCols, kServerColCount, n);
    paint_pane(cv, sp, kServerCols, kServerColCount,
               focused && g_tracker.focus_pane == 1);

    cv.set_clip(sp.body);
    for (int i = sp.vsb.value; i < n && sp.row_rect(i).y < sp.body.bottom();
         ++i) {
        const tracker::Room &s2 = g_tracker.dir.rooms[idx[i]];
        Rect row = sp.row_rect(i);
        if (idx[i] == g_tracker.sel_server)
            cv.fill({row.x, row.y + 1, row.w, row.h - 1}, kSelFill);
        int ty = row.y + (kRowH - kFontHeight) / 2 + 1;
        char cnt[16];
        wsprintfA(cnt, "%d", s2.users);
        draw_cell(cv, sp.col_x(kServerCols, 0), kServerCols[0].w, ty,
                  s2.name.c_str(), false);
        draw_cell(cv, sp.col_x(kServerCols, 1), kServerCols[1].w, ty, cnt,
                  true);
        draw_cell(cv, sp.col_x(kServerCols, 2), kServerCols[2].w, ty,
                  s2.date.c_str(), false);
        draw_cell(cv, sp.col_x(kServerCols, 3), kServerCols[3].w, ty,
                  s2.description.c_str(), false);
    }
    // While the directory is in flight (or unreachable) the list says so,
    // exactly where the rows would be.
    if (n == 0) {
        const char *msg = nullptr;
        std::string err;
        switch (g_tracker.dir.status) {
            case tracker::Fetching: msg = "Connecting to tracker..."; break;
            case tracker::Failed:
                err = g_tracker.dir.error;
                msg = err.c_str();
                break;
            case tracker::Ready: msg = "No servers in this group."; break;
            default: msg = "";
        }
        cv.text(sp.body.x + 6, sp.body.y + (kRowH - kFontHeight) / 2 + 1, msg,
                kGlyph);
    }
    cv.clear_clip();
    paint_grip(cv, lay.grip, focused, nullptr);
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
            ScrollBar *bars[] = {&g_tracker.groups.vsb, &g_tracker.groups.hsb,
                                 &g_tracker.servers.vsb,
                                 &g_tracker.servers.hsb};
            for (ScrollBar *sb : bars) {
                int hit = sb->on_press(x, y);
                if (hit == 2) {
                    g_tracker.drag = sb;
                    g_tracker.drag_grab =
                        (sb->vertical ? y - sb->thumb.y : x - sb->thumb.x);
                    SetCapture(hwnd);
                }
                if (hit) {
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
            }
            int g = g_tracker.groups.body.contains(x, y)
                        ? g_tracker.groups.row_at(y)
                        : -1;
            if (g >= 0 && g < kGroupCount) {
                g_tracker.focus_pane = 0;
                g_tracker.sel_group = g;
                g_tracker.sel_server = -1;
                g_tracker.servers.vsb.value = 0;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (g_tracker.servers.body.contains(x, y)) {
                g_tracker.focus_pane = 1;
                std::vector<int> idx = group_server_indices();
                int n = int(idx.size());
                int row = g_tracker.servers.row_at(y);
                g_tracker.sel_server = (row >= 0 && row < n) ? idx[row] : -1;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (g_tracker.lay.grip.contains(x, y))
                SendMessage(hwnd, WM_SYSCOMMAND, SC_SIZE + 8, lp);
            else if (y < g_tracker.lay.title_h)
                SendMessage(hwnd, WM_SYSCOMMAND, SC_MOVE + 2, lp);
            return 0;
        }
        case WM_SIZE:
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_TRACKER: {
            {
                Lock lock(g_tracker.mutex);
                g_tracker.dir = g_tracker.pending;
            }
            g_tracker.sel_server = -1;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_TIMER:
            refresh_tracker();
            return 0;
        case WM_MOUSEMOVE:
            if (g_tracker.drag) {
                int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
                g_tracker.drag->drag_to(
                    (g_tracker.drag->vertical ? y : x) - g_tracker.drag_grab);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case WM_MOUSEWHEEL: {
            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ScreenToClient(hwnd, &pt);
            int steps = GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;
            ListPane &p = g_tracker.groups.r.contains(pt.x, pt.y)
                              ? g_tracker.groups
                              : g_tracker.servers;
            p.vsb.set(p.vsb.value - steps * 3);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_LBUTTONUP:
            if (g_tracker.drag) {
                ReleaseCapture();
                g_tracker.drag = nullptr;
                return 0;
            }
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
            if (wp == VK_F5) refresh_tracker();
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
        InitializeCriticalSection(&g_tracker.mutex);
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
    refresh_tracker();
    SetTimer(g_tracker.hwnd, 1, 30000, nullptr);  // keep the list fresh
}

// ---- Host a Room ----

LRESULT CALLBACK host_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    host_room::Window &g = host_room::g;
    switch (msg) {
        case WM_NCCALCSIZE:
            if (wp) return 0;
            break;
        case WM_NCHITTEST:
            return HTCLIENT;
        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            if (g.lay.close_box.contains(x, y)) {
                DestroyWindow(hwnd);
                return 0;
            }
            for (int i = 0; i < 3; ++i)
                if (g.field[i].contains(x, y)) {
                    g.focus = i;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
            if (g.group_box.contains(x, y) && !g.hosting.active) {
                g.group = (g.group + 1) % kGroupCount;  // cycles the groups
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (g.action.contains(x, y) || g.close.contains(x, y)) {
                g.pressed_btn = g.action.contains(x, y) ? 0 : 1;
                SetCapture(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (y < g.lay.title_h)
                SendMessage(hwnd, WM_SYSCOMMAND, SC_MOVE + 2, lp);
            return 0;
        }
        case WM_LBUTTONUP: {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            if (g.pressed_btn >= 0) {
                ReleaseCapture();
                int was = g.pressed_btn;
                g.pressed_btn = -1;
                InvalidateRect(hwnd, nullptr, FALSE);
                if (was == 0 && g.action.contains(x, y)) {
                    if (g.hosting.active)
                        host_room::stop_hosting();
                    else
                        host_room::start_hosting();
                } else if (was == 1 && g.close.contains(x, y)) {
                    DestroyWindow(hwnd);
                }
            }
            return 0;
        }
        case WM_CHAR: {
            if (g.hosting.active) return 0;
            std::string &t = *host_room::focused_text();
            char c = char(wp);
            if (c == '\b') {
                if (!t.empty()) t.pop_back();
            } else if (c == '\t') {
                g.focus = (g.focus + 1) % 3;
            } else if (c == '\r') {
                host_room::start_hosting();
            } else if (c >= 32 && c < 127 && t.size() < 120) {
                if (g.focus != 2 || (c >= '0' && c <= '9')) t += c;
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) DestroyWindow(hwnd);
            return 0;
        case host_room::WM_HOST_DONE:
            g.busy = false;
            if (wp) {
                char buf[160];
                wsprintfA(buf, "Listed on the tracker on port %d.",
                          g.hosting.port);
                g.status = buf;
                SetTimer(hwnd, 2, 30000, nullptr);  // heartbeat
            } else {
                g.status = g.error.empty() ? "Registration failed."
                                           : g.error;
            }
            if (g_tracker.hwnd) refresh_tracker();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_TIMER:
            if (wp == 2) {
                tracker::heartbeat(g.hosting);
            } else {
                g.caret = !g.caret;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
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
            host_room::paint();
            blit_canvas(hdc, g.canvas);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            // Hosting outlives the window; the launcher keeps the heartbeat.
            g.hwnd = nullptr;
            if (g_main) SetForegroundWindow(g_main);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

void open_host_room(HINSTANCE hinst) {
    host_room::Window &g = host_room::g;
    if (g.hwnd) {
        SetForegroundWindow(g.hwnd);
        return;
    }
    static bool registered = false;
    if (!registered) {
        WNDCLASSA wc{};
        wc.lpfnWndProc = host_proc;
        wc.hInstance = hinst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = "SagradoHostRoom";
        RegisterClassA(&wc);
        registered = true;
    }
    RECT mr{};
    if (g_main) GetWindowRect(g_main, &mr);
    g.hwnd = CreateWindowExA(0, "SagradoHostRoom", "Host a Room", WS_POPUP,
                             mr.right + 12, mr.top + 40, host_room::kW,
                             host_room::kH, g_main, nullptr, hinst, nullptr);
    ShowWindow(g.hwnd, SW_SHOW);
    SetForegroundWindow(g.hwnd);
    SetTimer(g.hwnd, 1, 500, nullptr);  // caret blink
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
    if (lstrcmpA(name, "Commands") == 0) {
        open_host_room(g_hinst);  // the Commands menu starts with hosting
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
