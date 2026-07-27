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
#include "menu.h"
#include "room.h"
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
    int menu_btn = -1;  // command button holding its menu open
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
void start_session(room::Role role, const std::string &id,
                   const std::string &token, const std::string &name);

// Join whatever the server list has selected.
void join_selected() {
    int sel = g_tracker.sel_server;
    if (sel < 0 || sel >= int(g_tracker.dir.rooms.size())) return;
    const tracker::Room &r = g_tracker.dir.rooms[sel];
    if (r.id.empty()) return;
    start_session(room::Guest, r.id, "", r.name);
}

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
        case WM_LBUTTONDBLCLK:
            if (g_tracker.servers.body.contains(GET_X_LPARAM(lp),
                                                GET_Y_LPARAM(lp)))
                join_selected();
            return 0;
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) DestroyWindow(hwnd);
            if (wp == VK_F5) refresh_tracker();
            if (wp == VK_RETURN) join_selected();
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
        wc.style = CS_DBLCLKS;
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

// ---- Host a Server ----

void start_session(room::Role role, const std::string &id,
                   const std::string &token, const std::string &name);

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
            for (int i = 0; i < 2; ++i)
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
                g.focus = (g.focus + 1) % 2;
            } else if (c == '\r') {
                host_room::start_hosting();
            } else if (c >= 32 && c < 127 && t.size() < 120) {
                t += c;
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
                g.status = "Listed on the tracker.";
                start_session(room::Host, g.hosting.id, g.hosting.token,
                              g.hosting.name);
                DestroyWindow(hwnd);
                return 0;
            } else {
                g.status = g.error.empty() ? "Registration failed."
                                           : g.error;
            }
            if (g_tracker.hwnd) refresh_tracker();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_TIMER:
            g.caret = !g.caret;
            InvalidateRect(hwnd, nullptr, FALSE);
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
    g.hwnd = CreateWindowExA(0, "SagradoHostRoom", "Host a Server", WS_POPUP,
                             mr.right + 12, mr.top + 40, host_room::kW,
                             host_room::kH, g_main, nullptr, hinst, nullptr);
    ShowWindow(g.hwnd, SW_SHOW);
    SetForegroundWindow(g.hwnd);
    SetTimer(g.hwnd, 1, 500, nullptr);  // caret blink
}

// ---- Chat window ----
// The real KDX chat window: a Chat List button and the room's own tab along
// the top, a black chat pane where each line is drawn in the speaker's own
// colour, the user list on the right, the red-outlined entry box and the
// Topic button underneath. Host and guest use the same window; see room.h
// for the connection behind it.

constexpr int kSrvW = 800, kSrvH = 520;
constexpr int kUserW = 190;
constexpr int kLineH = kFontHeight + 2;
constexpr int kUserRowH = 24;
constexpr int kToolH = 22;   // Chat List / room tab row
constexpr int kEntryH = 40;  // the message box
constexpr int kTopicH = 20;

const Color kChatBg{0, 0, 0};
const Color kTabActive{136, 0, 0};

struct ServerWin {
    HWND hwnd = nullptr;
    Canvas canvas;
    ChromeLayout lay{};
    ScrollBar chat_sb, user_sb;
    Rect chat{}, users{}, entry{}, chat_list_btn{}, room_tab{}, topic_btn{};
    std::string draft;
    bool caret = true;
    int pressed_box = 0;
    int pressed_btn = -1;  // 0 Chat List, 1 room tab, 2 Topic
    ScrollBar *drag = nullptr;
    int drag_grab = 0;
    bool follow = true;  // stay pinned to the newest line
} g_server;

struct ChatLine {
    std::string text;
    Color fg;
};

// A sunken pane: black edge, dark face, the bevel the kit uses everywhere.
void sunken(Canvas &cv, Rect r, Color face) {
    cv.fill(r, face);
    cv.frame(r, kBlack);
    cv.hline(r.x + 1, r.right() - 1, r.y + 1, kPlateLo);
    cv.vline(r.x + 1, r.y + 1, r.bottom() - 1, kPlateLo);
    cv.hline(r.x + 1, r.right() - 1, r.bottom() - 2, kPlateHi);
    cv.vline(r.right() - 2, r.y + 1, r.bottom() - 1, kPlateHi);
}

// Program art carries the launcher's #333 panel behind it; treat that as
// transparent so an icon can sit on any background.
void blit_icon(Canvas &cv, const ArtImage &a, int x, int y) {
    for (int iy = 0; iy < a.h; ++iy)
        for (int ix = 0; ix < a.w; ++ix) {
            uint32_t p = a.px[size_t(iy) * a.w + ix];
            if ((p & 0xffffff) != 0x333333) cv.put(x + ix, y + iy, p);
        }
}

// Break the log into display lines at the pane's width, on spaces when it
// can and mid-word when a word is longer than the pane. Each display line
// keeps the colour of the line it came from.
std::vector<ChatLine> wrap_log(Canvas &cv, const std::vector<room::Line> &log,
                               int width) {
    std::vector<ChatLine> out;
    for (const room::Line &line : log) {
        Color fg = from_u32(line.fg);
        std::string cur;
        size_t last_space = std::string::npos;
        for (char c : line.text) {
            cur += c;
            if (c == ' ') last_space = cur.size() - 1;
            if (cv.text_width(cur.c_str()) <= width) continue;
            if (last_space != std::string::npos && last_space > 0) {
                out.push_back({cur.substr(0, last_space), fg});
                cur = cur.substr(last_space + 1);
            } else {
                out.push_back({cur.substr(0, cur.size() - 1), fg});
                cur = cur.substr(cur.size() - 1);
            }
            last_space = std::string::npos;
        }
        out.push_back({cur, fg});
    }
    return out;
}

// The room's own tab: the same button shape held red, as KDX marks the chat
// you are looking at.
void draw_tab(Canvas &cv, Rect r, const char *label, bool pressed,
              const DialogColors &dc) {
    cv.fill(r, kTabActive);
    rounded_frame(cv, r, dc.btn_frame, dc.workspace);
    Color hi = pressed ? Color{68, 0, 0} : Color{204, 0, 0};
    Color lo = pressed ? Color{204, 0, 0} : Color{68, 0, 0};
    cv.hline(r.x + 1, r.right() - 1, r.y + 1, hi);
    cv.vline(r.x + 1, r.y + 1, r.bottom() - 1, hi);
    cv.hline(r.x + 1, r.right() - 1, r.bottom() - 2, lo);
    cv.vline(r.right() - 2, r.y + 1, r.bottom() - 1, lo);
    int off = pressed ? 1 : 0;
    blit_icon(cv, kIcChat, r.x + 6 + off, r.y + (r.h - kIcChat.h) / 2 + off);
    cv.text(r.x + 26 + off, r.y + (r.h - kFontHeight) / 2 + off, label,
            kWhite);
}

void paint_server() {
    ServerWin &s = g_server;
    Canvas &cv = s.canvas;
    RECT rc;
    GetClientRect(s.hwnd, &rc);
    if (rc.right <= 0 || rc.bottom <= 0) return;
    if (cv.width() != rc.right || cv.height() != rc.bottom)
        cv.resize(rc.right, rc.bottom);
    bool focused = GetForegroundWindow() == s.hwnd;
    s.lay = chrome_layout(rc.right, rc.bottom, nullptr, focused);
    s.lay.max_box = {0, 0, 0, 0};

    std::string name = room::name_copy();
    if (name.empty()) name = "Sagrado Server";
    std::string title = "Chat: " + name;
    paint_chrome(cv, s.lay, title.c_str(), focused, 0,
                 s.pressed_box == 5 ? 1 : 0, nullptr);

    DialogColors dc = dialog_colors(nullptr);
    Rect cl = s.lay.client;
    cv.fill(cl, dc.workspace);

    s.chat_list_btn = {cl.x + 6, cl.y + 6, 92, kToolH};
    int tab_w = cv.text_width(name.c_str()) + 36;
    s.room_tab = {s.chat_list_btn.right() + 8, cl.y + 6, tab_w, kToolH};
    s.topic_btn = {cl.x + 6, cl.bottom() - 6 - kTopicH, 62, kTopicH};
    int top = s.chat_list_btn.bottom() + 6;
    int right_col = cl.right() - 6 - kUserW;
    s.entry = {cl.x + 6, s.topic_btn.y - 6 - kEntryH,
               right_col - 8 - (cl.x + 6), kEntryH};
    s.chat = {cl.x + 6, top, s.entry.w, s.entry.y - 6 - top};
    s.users = {right_col, top, kUserW, s.entry.bottom() - top};

    draw_button(cv, s.chat_list_btn, "Chat List", s.pressed_btn == 0, false,
                dc);
    draw_tab(cv, s.room_tab, name.c_str(), s.pressed_btn == 1, dc);

    std::vector<room::Line> log = room::log_copy();
    std::vector<ChatLine> lines = wrap_log(cv, log, s.chat.w - kSbW - 10);
    int rows = (s.chat.h - 4) / kLineH;
    s.chat_sb.vertical = true;
    s.chat_sb.r = {s.chat.right() - kSbW, s.chat.y, kSbW, s.chat.h};
    s.chat_sb.page = rows > 0 ? rows : 1;
    s.chat_sb.total = int(lines.size());
    if (s.follow) s.chat_sb.value = s.chat_sb.max_value();
    s.chat_sb.layout();

    sunken(cv, s.chat, kChatBg);
    cv.set_clip({s.chat.x + 2, s.chat.y + 2, s.chat.w - kSbW - 2,
                 s.chat.h - 4});
    for (int i = 0; i < rows; ++i) {
        int idx = s.chat_sb.value + i;
        if (idx < 0 || idx >= int(lines.size())) break;
        cv.text(s.chat.x + 5, s.chat.y + 3 + i * kLineH,
                lines[idx].text.c_str(), lines[idx].fg);
    }
    cv.clear_clip();
    s.chat_sb.paint(cv);

    // Each user gets a row of their own colours, with their icon beside the
    // name (one icon for everyone until Settings can set it).
    std::vector<room::User> users = room::user_copy();
    int seats = (s.users.h - 4) / kUserRowH;
    s.user_sb.vertical = true;
    s.user_sb.r = {s.users.right() - kSbW, s.users.y, kSbW, s.users.h};
    s.user_sb.page = seats > 0 ? seats : 1;
    s.user_sb.total = int(users.size());
    s.user_sb.layout();

    sunken(cv, s.users, kChatBg);
    Rect body{s.users.x + 2, s.users.y + 2, s.users.w - kSbW - 2,
              s.users.h - 4};
    cv.set_clip(body);
    for (int i = 0; i < seats; ++i) {
        int idx = s.user_sb.value + i;
        if (idx < 0 || idx >= int(users.size())) break;
        const room::User &u = users[idx];
        Rect row{body.x, body.y + i * kUserRowH, body.w, kUserRowH};
        cv.fill(row, from_u32(u.bg));
        cv.hline(row.x, row.right(), row.bottom() - 1, kBarBody);
        blit_icon(cv, kIcUsers, row.x + 4, row.y + (kUserRowH - kIcUsers.h) / 2);
        cv.text(row.x + 26, row.y + (kUserRowH - kFontHeight) / 2,
                u.nick.c_str(), from_u32(u.fg));
    }
    cv.clear_clip();
    s.user_sb.paint(cv);

    // The entry box: black, with the red focus outline the real one has.
    cv.fill(s.entry, kChatBg);
    cv.frame(s.entry, focused ? dc.field_focus : dc.field_frame);
    int end = cv.text(s.entry.x + 5, s.entry.y + 4, s.draft.c_str(),
                      from_u32(room::g.me.fg));
    if (focused && s.caret)
        cv.vline(end + 1, s.entry.y + 3, s.entry.y + 3 + kFontHeight,
                 dc.caret);

    draw_button(cv, s.topic_btn, "Topic:", s.pressed_btn == 2, false, dc);
    std::string status = room::status_copy();
    if (!status.empty()) {
        Rect line{s.topic_btn.right() + 8, s.topic_btn.y,
                  cl.right() - 6 - (s.topic_btn.right() + 8), kTopicH};
        cv.set_clip(line);
        cv.text(line.x, line.y + (kTopicH - kFontHeight) / 2, status.c_str(),
                dc.label);
        cv.clear_clip();
    }
    paint_grip(cv, s.lay.grip, focused, nullptr);
}

void close_session() {
    room::leave();
    if (host_room::g.hosting.active) host_room::stop_hosting();
}

LRESULT CALLBACK server_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    ServerWin &s = g_server;
    switch (msg) {
        case WM_NCCALCSIZE:
            if (wp) return 0;
            break;
        case WM_NCHITTEST:
            return HTCLIENT;
        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            if (s.lay.close_box.contains(x, y)) {
                s.pressed_box = 5;
                SetCapture(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (s.lay.min_box.contains(x, y)) {
                CloseWindow(hwnd);
                return 0;
            }
            int hit = s.chat_sb.on_press(x, y);
            if (hit) {
                s.follow = s.chat_sb.value >= s.chat_sb.max_value();
                if (hit == 2) {
                    s.drag = &s.chat_sb;
                    s.drag_grab = y - s.chat_sb.thumb.y;
                    SetCapture(hwnd);
                }
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            hit = s.user_sb.on_press(x, y);
            if (hit) {
                if (hit == 2) {
                    s.drag = &s.user_sb;
                    s.drag_grab = y - s.user_sb.thumb.y;
                    SetCapture(hwnd);
                }
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            // The chat list and topic buttons press but have nothing behind
            // them until multiple chats and topics exist.
            int btn = s.chat_list_btn.contains(x, y)  ? 0
                      : s.room_tab.contains(x, y)     ? 1
                      : s.topic_btn.contains(x, y)    ? 2
                                                      : -1;
            if (btn >= 0) {
                s.pressed_btn = btn;
                SetCapture(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (s.lay.grip.contains(x, y))
                SendMessage(hwnd, WM_SYSCOMMAND, SC_SIZE + 8, lp);
            else if (y < s.lay.title_h)
                SendMessage(hwnd, WM_SYSCOMMAND, SC_MOVE + 2, lp);
            return 0;
        }
        case WM_MOUSEMOVE:
            if (s.drag) {
                s.drag->drag_to(GET_Y_LPARAM(lp) - s.drag_grab);
                if (s.drag == &s.chat_sb)
                    s.follow = s.chat_sb.value >= s.chat_sb.max_value();
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case WM_LBUTTONUP:
            if (s.drag) {
                ReleaseCapture();
                s.drag = nullptr;
                return 0;
            }
            if (s.pressed_box == 5) {
                ReleaseCapture();
                s.pressed_box = 0;
                if (s.lay.close_box.contains(GET_X_LPARAM(lp),
                                             GET_Y_LPARAM(lp))) {
                    DestroyWindow(hwnd);
                    return 0;
                }
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            if (s.pressed_btn >= 0) {
                ReleaseCapture();
                s.pressed_btn = -1;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case WM_MOUSEWHEEL: {
            int steps = GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;
            POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ScreenToClient(hwnd, &p);
            if (s.users.contains(p.x, p.y)) {
                s.user_sb.set(s.user_sb.value - steps * 3);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            s.chat_sb.set(s.chat_sb.value - steps * 3);
            s.follow = s.chat_sb.value >= s.chat_sb.max_value();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_CHAR: {
            char c = char(wp);
            if (c == '\b') {
                if (!s.draft.empty()) s.draft.pop_back();
            } else if (c == '\r') {
                room::say(s.draft);
                s.draft.clear();
                s.follow = true;
            } else if (c >= 32 && c < 127 && s.draft.size() < 400) {
                s.draft += c;
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) DestroyWindow(hwnd);
            return 0;
        case room::WM_ROOM_EVENT:
        case WM_SIZE:
        case WM_ACTIVATE:
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_TIMER:
            s.caret = !s.caret;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            paint_server();
            blit_canvas(hdc, s.canvas);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            close_session();
            s.hwnd = nullptr;
            if (g_main) {
                SetForegroundWindow(g_main);
                InvalidateRect(g_main, nullptr, FALSE);
            }
            return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

HWND open_server_window(HINSTANCE hinst);

// Open (or reuse) the server window and point a session at it.
void start_session(room::Role role, const std::string &id,
                   const std::string &token, const std::string &name) {
    HWND w = open_server_window(g_hinst);
    room::start(role, w, id, token, name);
    InvalidateRect(w, nullptr, FALSE);
}

// Bring the public chat window forward. Real KDX's Chat command opens the
// lobby for the server you are on; if you are not connected yet, Connect...
// is the way in.
void open_chat() {
    if (g_server.hwnd) {
        ShowWindow(g_server.hwnd, SW_RESTORE);
        SetForegroundWindow(g_server.hwnd);
        return;
    }
    if (room::g.running || room::g.connected) {
        open_server_window(g_hinst);
        return;
    }
    open_tracker(g_hinst);
}

HWND open_server_window(HINSTANCE hinst) {
    ServerWin &s = g_server;
    if (s.hwnd) {
        SetForegroundWindow(s.hwnd);
        return s.hwnd;
    }
    static bool registered = false;
    if (!registered) {
        WNDCLASSA wc{};
        wc.lpfnWndProc = server_proc;
        wc.hInstance = hinst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = "SagradoServer";
        RegisterClassA(&wc);
        registered = true;
    }
    s.draft.clear();
    s.follow = true;
    RECT mr{};
    if (g_main) GetWindowRect(g_main, &mr);
    s.hwnd = CreateWindowExA(0, "SagradoServer", "Sagrado Server", WS_POPUP,
                             mr.right + 12, mr.top, kSrvW, kSrvH, nullptr,
                             nullptr, hinst, nullptr);
    ShowWindow(s.hwnd, SW_SHOW);
    SetForegroundWindow(s.hwnd);
    SetTimer(s.hwnd, 1, 500, nullptr);  // caret blink
    return s.hwnd;
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

// A command button while its menu is down: the red hilite face the real
// client shows, lit from the top and shading toward the bottom.
void draw_command_hilite(Canvas &cv, int i, const DialogColors &dc) {
    Rect r = command_rect(i);
    cv.frame(r, dc.btn_frame);
    cv.hline(r.x + 1, r.right() - 1, r.y + 1, kBright);
    cv.hline(r.x + 1, r.right() - 1, r.y + 2, Color{170, 0, 0});
    cv.vgradient({r.x + 1, r.y + 3, r.w - 2, r.h - 4}, Color{136, 0, 0},
                 Color{85, 0, 0});
    blit_art(cv, *kCommands[i].icon);
    cv.text(36, r.y + (r.h - kFontHeight) / 2, kCommands[i].label, kWhite);
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

    for (int i = 0; i < kCommandCount; ++i) {
        if (i == g_app.menu_btn)
            draw_command_hilite(cv, i, dc);
        else
            draw_command(cv, i, g_app.pressed_btn == i, dc);
    }

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

// --- Commands menu -------------------------------------------------------
// Clicking Commands drops a menu beside the button, as in the real client.
// Entries that need a room connection stay dimmed until that lands.

enum CommandId {
    CmdHostRoom = 1,
    CmdStopHosting,
    CmdConnect,
    CmdAddressBook,
    CmdMessages,
    CmdTransfers,
    CmdBrowser,
    CmdChat,
    CmdNews,
    CmdUserList,
    CmdSettings,
    CmdShowAddress,
    CmdAbout,
    CmdExit,
};

menu::Icon icon_of(const ArtImage &a) { return {a.w, a.h, a.px}; }

void menu_closed() {
    g_app.menu_btn = -1;
    g_app.pressed_btn = -1;
    if (g_main) InvalidateRect(g_main, nullptr, FALSE);
}

void menu_chosen(int id) {
    switch (id) {
        case CmdHostRoom:
            open_host_room(g_hinst);
            break;
        case CmdStopHosting:
            host_room::stop_hosting();
            break;
        case CmdConnect:
            open_tracker(g_hinst);
            break;
        case CmdChat:
            open_chat();
            break;
        case CmdAbout:
            MessageBoxA(g_main,
                        "Sagrado KDX\n\nA modern peer-to-peer client in the "
                        "Haxial KDX tradition.",
                        "About Sagrado KDX", MB_OK);
            break;
        case CmdExit:
            if (g_main) DestroyWindow(g_main);
            break;
        default:
            break;
    }
}

// Laid out like the real client's menu: an icon column, right-aligned
// shortcuts and one engraved divider under the hosting commands. Entries that
// need a room connection stay dimmed until that milestone lands.
void open_commands_menu(int button) {
    bool hosting = host_room::g.hosting.active;
    std::vector<menu::Item> items{
        {"Host a Server...", CmdHostRoom, !hosting, icon_of(kIcCommands), "^R",
         false},
        {"Stop Hosting", CmdStopHosting, hosting, {}, "", false},
        menu::separator(),
        {"Connect...", CmdConnect, true, icon_of(kIcConnect), "^K", false},
        {"Address Book", CmdAddressBook, false, icon_of(kIcAddress), "^B",
         false},
        {"Messages", CmdMessages, false, icon_of(kIcMessages), "^M", false},
        {"File Transfers", CmdTransfers, false, icon_of(kIcTransfers), "^T",
         false},
        {"File Browser", CmdBrowser, false, icon_of(kIcBrowser), "^F", false},
        {"Chat", CmdChat, true, icon_of(kIcChat), "^H", false},
        {"News", CmdNews, false, icon_of(kIcNews), "^N", false},
        {"User List", CmdUserList, false, icon_of(kIcUsers), "^U", false},
        {"Settings", CmdSettings, false, icon_of(kIcSettings), "^;", false},
        {"Show My Address", CmdShowAddress, false, {}, "", false},
        {"About", CmdAbout, true, icon_of(kIcAbout), "", false},
        {"Exit", CmdExit, true, icon_of(kIcExit), "^Q", false},
    };
    Rect b = command_rect(button);
    POINT p{b.x + 16, b.bottom() + 2};
    ClientToScreen(g_main, &p);
    menu::open(g_hinst, g_main, p.x, p.y, std::move(items), menu_chosen,
               menu_closed);
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
    if (lstrcmpA(name, "Chat") == 0) {
        open_chat();
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
                InvalidateRect(hwnd, nullptr, FALSE);
                // Menu buttons drop their menu on press and stay held down
                // while it is up; the menu takes the mouse capture.
                if (lstrcmpA(kCommands[b].label, "Commands") == 0) {
                    g_app.menu_btn = b;
                    open_commands_menu(b);
                } else {
                    SetCapture(hwnd);
                }
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
            if (GetKeyState(VK_CONTROL) & 0x8000) {
                if (wp == 'K') open_tracker(g_hinst);
                if (wp == 'R') open_host_room(g_hinst);
                if (wp == 'H') open_chat();
                if (wp == 'Q') DestroyWindow(hwnd);
            }
            return 0;
        case WM_TIMER: {
            if (wp == 2) {  // keeps our tracker listing from lapsing
                tracker::heartbeat(host_room::g.hosting);
                return 0;
            }
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
    room::init();
    ShowWindow(hwnd, show);
    SetTimer(hwnd, 1, 250, nullptr);
    SetTimer(hwnd, 2, 30000, nullptr);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
