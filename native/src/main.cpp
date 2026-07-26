// Sagrado TextEdit — native Win32, drawn entirely into a software
// framebuffer and blitted with SetDIBitsToDevice, the way Haxial built KDX.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#include "canvas.h"
#include "chrome.h"

namespace {

constexpr int kMenuH = 20; // measured: menu bar rows 22..41
constexpr int kTabH = 33;  // measured: tab strip rows 42..74

const char *kMenus[] = {"File", "Tools", "Favorites", "Location",
                        "Appearance"};
const char *kMenuItems[5][6] = {
    {"New", "Open...", "Save", "Save As...", "Close", "Quit"},
    {"Find & Replace...", "Sort Lines", "Count Occurrences", nullptr},
    {"Add Favorite", "Show Favorites", nullptr},
    {"Documents Folder", "Desktop", nullptr},
    {"Haxial Standard", nullptr},
};

struct App {
    Canvas canvas;
    ChromeLayout lay{};
    bool focused = true;
    int pressed_box = 0; // 1 close, 3 max, 4 min
    int hot_box = 0;
    Rect menu_rects[5]{};
    int open_menu = -1;
    int hot_item = -1;
    Rect dropdown{};
    int item_h = 18;
    int drag_mode = 0; // 0 none, 3 thumb drag, 4 title boxes
    int thumb_grab = 0;
    // Scrolling.
    int scroll = 0;      // first visible line
    int total_lines = 0; // document length
    int page_lines = 1;
    Rect sb{}, up1{}, dn1{}, up2{}, dn2{}, thumb{}, track{};
} g_app;

constexpr int kLineH = kFontHeight + 1;

int max_scroll() {
    int m = g_app.total_lines - g_app.page_lines;
    return m > 0 ? m : 0;
}

void set_scroll(int v) {
    int m = max_scroll();
    g_app.scroll = v < 0 ? 0 : (v > m ? m : v);
}

void paint_content(Canvas &cv, const ChromeLayout &lay) {
    Rect c = lay.client;

    // Menu bar.
    Rect menu{c.x, c.y, c.w, kMenuH};
    raised_bar(cv, menu);
    int x = menu.x + 8;
    for (int i = 0; i < 5; ++i) {
        int tw = cv.text_width(kMenus[i]);
        g_app.menu_rects[i] = {x, menu.y, tw + 12, kMenuH - 2};
        if (g_app.open_menu == i) cv.fill(g_app.menu_rects[i], kBody);
        cv.text(x + 6, menu.y + 2, kMenus[i], kWhite);
        x += tw + 12;
    }
    // Save-state indicator triangle at the right of the menu bar.
    int tx = menu.right() - 16;
    for (int i = 0; i < 6; ++i)
        cv.hline(tx - i, tx + i + 1, menu.y + 4 + i, kGlyphGrey);

    // Tab strip with one active red tab (measured: 33px strip, tab 4px
    // below its top, 24px tall).
    Rect strip{c.x, menu.bottom(), c.w, kTabH};
    raised_bar(cv, strip);
    Rect tab{strip.x + 4, strip.y + 4, 108, 24};
    bevel_box(cv, tab, false);
    // Save-state square icon, then the document name.
    cv.fill({tab.x + 8, tab.y + 7, 10, 10}, Color{186, 118, 118});
    cv.frame({tab.x + 8, tab.y + 7, 10, 10}, kDeep);
    cv.text(tab.x + 24, tab.y + 4, "Untitled", kWhite);

    // Editor area: black, scrollable sample document.
    Rect editor{c.x, strip.bottom(), c.w - kScrollbar,
                c.bottom() - strip.bottom()};
    cv.fill({c.x, strip.bottom(), c.w, c.bottom() - strip.bottom()}, kBlack);
    static const char *kPoem[] = {
        "'Twas brillig, and the slithy toves",
        "Did gyre and gimble in the wabe;",
        "All mimsy were the borogoves,",
        "And the mome raths outgrabe.",
        "",
        "'Beware the Jabberwock, my son!",
        "The jaws that bite, the claws that catch!",
        "Beware the Jubjub bird, and shun",
        "The frumious Bandersnatch!'",
        "",
        "He took his vorpal sword in hand:",
        "Long time the manxome foe he sought--",
        "So rested he by the Tumtum tree,",
        "And stood awhile in thought.",
        "",
        "And as in uffish thought he stood,",
        "The Jabberwock, with eyes of flame,",
        "Came whiffling through the tulgey wood,",
        "And burbled as it came!",
        "",
        "One, two! One, two! And through and through",
        "The vorpal blade went snicker-snack!",
        "He left it dead, and with its head",
        "He went galumphing back.",
        "",
        "'And hast thou slain the Jabberwock?",
        "Come to my arms, my beamish boy!",
        "O frabjous day! Callooh! Callay!'",
        "He chortled in his joy.",
        "",
        "'Twas brillig, and the slithy toves",
        "Did gyre and gimble in the wabe;",
        "All mimsy were the borogoves,",
        "And the mome raths outgrabe.",
    };
    g_app.total_lines = int(sizeof(kPoem) / sizeof(kPoem[0]));
    g_app.page_lines = (editor.h - 8) / kLineH;
    if (g_app.page_lines < 1) g_app.page_lines = 1;
    set_scroll(g_app.scroll);
    int y = editor.y + 4;
    for (int i = g_app.scroll;
         i < g_app.total_lines && y + kFontHeight <= editor.bottom() - 2;
         ++i) {
        cv.text(editor.x + 4, y, kPoem[i], Color{0, 204, 0});
        y += kLineH;
    }

    // Vertical scrollbar, stopping above the grow box.
    Rect sb{c.right() - kScrollbar + 1, editor.y, kScrollbar,
            g_app.lay.grip.y - editor.y};
    g_app.sb = sb;
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
        return b;
    };
    int ah = 13;
    g_app.up1 = arrow_box(sb.y, ah, true);
    g_app.dn1 = arrow_box(sb.y + ah - 1, ah, false);
    g_app.up2 = arrow_box(sb.bottom() - 2 * ah + 1, ah, true);
    g_app.dn2 = arrow_box(sb.bottom() - ah, ah, false);
    // Thumb, positioned from the scroll state.
    Rect track{sb.x, g_app.dn1.bottom() - 1, sb.w,
               g_app.up2.y - g_app.dn1.bottom() + 2};
    g_app.track = track;
    int th = max_scroll() > 0
                 ? track.h * g_app.page_lines / g_app.total_lines
                 : track.h;
    if (th < 20) th = 20;
    if (th > track.h) th = track.h;
    int ty = track.y;
    if (max_scroll() > 0)
        ty += (track.h - th) * g_app.scroll / max_scroll();
    Rect thumb{sb.x, ty, sb.w, th};
    g_app.thumb = thumb;
    cv.fill(thumb, kThumb);
    cv.frame(thumb, kBlack);
    cv.hline(thumb.x + 1, thumb.right() - 1, thumb.y + 1, kThumbHi);
    cv.vline(thumb.x + 1, thumb.y + 1, thumb.bottom() - 1, kThumbHi);
    if (th >= 14)
        for (int i = 0; i < 3; ++i)
            cv.hline(thumb.x + 4, thumb.right() - 4,
                     thumb.y + th / 2 - 3 + i * 3, kBarDark);
}

int menu_item_count(int m) {
    int n = 0;
    while (n < 6 && kMenuItems[m][n]) ++n;
    return n;
}

// The open pull-down menu: dark raised panel, red hilite bar, painted last.
void paint_dropdown(Canvas &cv) {
    int m = g_app.open_menu;
    if (m < 0) return;
    int n = menu_item_count(m);
    int wmax = 0;
    for (int i = 0; i < n; ++i) {
        int tw = cv.text_width(kMenuItems[m][i]);
        if (tw > wmax) wmax = tw;
    }
    Rect r{g_app.menu_rects[m].x, g_app.menu_rects[m].bottom() + 2, wmax + 24,
           n * g_app.item_h + 4};
    g_app.dropdown = r;
    cv.fill(r, kBarBody);
    cv.frame(r, kBlack);
    cv.hline(r.x + 1, r.right() - 1, r.y + 1, kBarLight);
    cv.vline(r.x + 1, r.y + 1, r.bottom() - 1, kBarLight);
    cv.hline(r.x + 1, r.right() - 1, r.bottom() - 2, kBarDark);
    cv.vline(r.right() - 2, r.y + 1, r.bottom() - 1, kBarDark);
    for (int i = 0; i < n; ++i) {
        Rect item{r.x + 2, r.y + 2 + i * g_app.item_h, r.w - 4, g_app.item_h};
        if (i == g_app.hot_item) cv.fill(item, kBody);
        cv.text(item.x + 10, item.y + 1, kMenuItems[m][i], kWhite);
    }
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
    paint_grip(cv, g_app.lay.grip, g_app.focused);
    paint_dropdown(cv);
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
            if (box_at(x, y) || g_app.open_menu >= 0) return HTCLIENT;
            (void)lay;
            // Everything is handled manually in the client area so drag
            // and resize behave identically on Windows and Wine.
            return HTCLIENT;
        }
        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            if (g_app.open_menu >= 0) {
                if (g_app.dropdown.contains(x, y)) {
                    int i = (y - g_app.dropdown.y - 2) / g_app.item_h;
                    int m = g_app.open_menu;
                    g_app.open_menu = -1;
                    if (i >= 0 && i < menu_item_count(m) && m == 0 &&
                        (i == 4 || i == 5))
                        DestroyWindow(hwnd); // Close / Quit
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                g_app.open_menu = -1;
                InvalidateRect(hwnd, nullptr, FALSE);
                // fall through to allow clicking another menu title
            }
            for (int i = 0; i < 5; ++i)
                if (g_app.menu_rects[i].contains(x, y)) {
                    g_app.open_menu = i;
                    g_app.hot_item = -1;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
            int b = box_at(x, y);
            if (b) {
                g_app.pressed_box = b;
                g_app.drag_mode = 4;
                SetCapture(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            // Scrollbar interaction.
            if (g_app.sb.contains(x, y)) {
                if (g_app.up1.contains(x, y) || g_app.up2.contains(x, y))
                    set_scroll(g_app.scroll - 1);
                else if (g_app.dn1.contains(x, y) || g_app.dn2.contains(x, y))
                    set_scroll(g_app.scroll + 1);
                else if (g_app.thumb.contains(x, y)) {
                    g_app.drag_mode = 3;
                    g_app.thumb_grab = y - g_app.thumb.y;
                    SetCapture(hwnd);
                } else if (y < g_app.thumb.y)
                    set_scroll(g_app.scroll - g_app.page_lines);
                else
                    set_scroll(g_app.scroll + g_app.page_lines);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            // Native move / resize: hand off to Windows' own modal
            // move/size loop (what DefWindowProc runs for SC_MOVE/SC_SIZE).
            if (g_app.lay.grip.contains(x, y) ||
                (x >= g_app.lay.window.w - kBorder &&
                 y >= g_app.lay.window.h - kBorder)) {
                SendMessage(hwnd, WM_SYSCOMMAND, SC_SIZE + 8 /*bottomright*/,
                            lp);
            } else if (y < kTitleH) {
                SendMessage(hwnd, WM_SYSCOMMAND, SC_MOVE + 2 /*via mouse*/,
                            lp);
            }
            return 0;
        }
        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            if (g_app.drag_mode == 3) {
                int span = g_app.track.h - g_app.thumb.h;
                if (span > 0 && max_scroll() > 0) {
                    int ty = y - g_app.thumb_grab - g_app.track.y;
                    set_scroll((ty * max_scroll() + span / 2) / span);
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            }
            if (g_app.open_menu >= 0) {
                int hot = g_app.dropdown.contains(x, y)
                              ? (y - g_app.dropdown.y - 2) / g_app.item_h
                              : -1;
                if (hot != g_app.hot_item) {
                    g_app.hot_item = hot;
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
            }
            return 0;
        }
        case WM_MOUSEWHEEL: {
            set_scroll(g_app.scroll -
                       GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA * 3);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE && g_app.open_menu >= 0) {
                g_app.open_menu = -1;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case WM_LBUTTONUP: {
            if (g_app.drag_mode) {
                int mode = g_app.drag_mode;
                g_app.drag_mode = 0;
                if (mode != 4) {
                    ReleaseCapture();
                    return 0;
                }
            }
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
        case WM_ACTIVATEAPP:
            g_app.focused = wp != FALSE;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            g_app.focused = msg == WM_SETFOCUS;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_TIMER: {
            bool f = GetForegroundWindow() == hwnd;
            if (f != g_app.focused) {
                g_app.focused = f;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
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

    // Plain popup: no WS_CAPTION/WS_THICKFRAME, so no window manager under
    // Wine adds its own decorations. Move/resize are handled manually.
    HWND hwnd = CreateWindowExA(
        0, "SagradoWindow", "Sagrado TextEdit",
        WS_POPUP | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 760, 520, nullptr, nullptr, hinst,
        nullptr);
    ShowWindow(hwnd, show);
    // Focus watchdog: some window managers (Wine/X11) don't deliver
    // WM_ACTIVATE reliably to popup windows, so poll the foreground state.
    SetTimer(hwnd, 1, 250, nullptr);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
