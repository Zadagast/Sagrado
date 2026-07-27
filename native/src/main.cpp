// Sagrado TextEdit — native Win32, drawn entirely into a software
// framebuffer and blitted with SetDIBitsToDevice, the way Haxial built KDX.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#include <string>
#include <vector>

#include "canvas.h"
#include "chrome.h"
#include "editor.h"

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
    {nullptr}, // Appearance: built dynamically from discovered .hap files
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
    int dd_rows = 1, dd_colw = 1; // dropdown column layout
    int dd_scroll = 0;            // first visible column (long menus scroll)
    int dd_cols_total = 1;        // total columns before clamping to width
    int dd_cols_shown = 1;        // columns that fit in the window
    int drag_mode = 0; // 0 none, 3 thumb drag, 4 title boxes
    int thumb_grab = 0;
    // Scrolling.
    int scroll = 0;      // first visible line
    int total_lines = 0; // document length
    int page_lines = 1;
    Rect sb{}, up1{}, dn1{}, up2{}, dn2{}, thumb{}, track{};
    // Editing.
    TextDoc doc;
    Rect editor{};       // last painted editor rect (for hit-testing)
    bool selecting = false;
    bool caret_on = true;
    // Appearance (.hap) support.
    Theme theme;
    bool themed = false;
    int theme_index = 0; // 0 = Haxial Standard (built-in primitives)
    std::vector<std::string> hap_names, hap_paths;
} g_app;

// Find .hap files near the executable (the repo's themes/Appearances
// folder, or an Appearances folder next to the program like Haxial).
void discover_haps() {
    char exe[MAX_PATH];
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    std::string dir(exe);
    size_t cut = dir.find_last_of("\\/");
    dir = cut == std::string::npos ? "." : dir.substr(0, cut);
    const char *cands[] = {"\\Appearances", "\\themes\\Appearances",
                           "\\..\\themes\\Appearances",
                           "\\..\\..\\themes\\Appearances"};
    for (const char *c : cands) {
        std::string base = dir + c;
        WIN32_FIND_DATAA fd;
        HANDLE fh = FindFirstFileA((base + "\\*.hap").c_str(), &fd);
        if (fh == INVALID_HANDLE_VALUE) continue;
        do {
            std::string name(fd.cFileName);
            g_app.hap_paths.push_back(base + "\\" + name);
            g_app.hap_names.push_back(name.substr(0, name.size() - 4));
        } while (FindNextFileA(fh, &fd));
        FindClose(fh);
        if (!g_app.hap_paths.empty()) break;
    }
}

void select_theme(int index) {
    g_app.theme_index = index;
    g_app.themed = false;
    if (index > 0 && index <= int(g_app.hap_paths.size())) {
        Theme t;
        if (load_hap(g_app.hap_paths[index - 1], t)) {
            g_app.theme = std::move(t);
            g_app.themed = true;
        }
    }
}

const Theme *active_theme() { return g_app.themed ? &g_app.theme : nullptr; }

constexpr int kLineH = kFontHeight + 1;

int max_scroll() {
    int m = g_app.total_lines - g_app.page_lines;
    return m > 0 ? m : 0;
}

void set_scroll(int v) {
    int m = max_scroll();
    g_app.scroll = v < 0 ? 0 : (v > m ? m : v);
}

// Keep the caret line within the visible page.
void ensure_caret_visible() {
    Pos car = g_app.doc.caret();
    if (car.line < g_app.scroll)
        g_app.scroll = car.line;
    else if (car.line >= g_app.scroll + g_app.page_lines)
        g_app.scroll = car.line - g_app.page_lines + 1;
    if (g_app.scroll < 0) g_app.scroll = 0;
}

// Restart the caret blink in the "on" phase after any edit or move.
void wake_caret() { g_app.caret_on = true; }

void clipboard_copy(HWND hwnd, const std::string &s) {
    if (s.empty() || !OpenClipboard(hwnd)) return;
    EmptyClipboard();
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, s.size() + 1);
    if (h) {
        char *p = static_cast<char *>(GlobalLock(h));
        memcpy(p, s.c_str(), s.size() + 1);
        GlobalUnlock(h);
        SetClipboardData(CF_TEXT, h);
    }
    CloseClipboard();
}

std::string clipboard_paste(HWND hwnd) {
    std::string out;
    if (!OpenClipboard(hwnd)) return out;
    HANDLE h = GetClipboardData(CF_TEXT);
    if (h) {
        const char *p = static_cast<const char *>(GlobalLock(h));
        if (p) out = p;
        GlobalUnlock(h);
    }
    CloseClipboard();
    return out;
}

// Document position under a client point (clamped into the editor).
Pos pos_at(int px, int py) {
    const Rect &e = g_app.editor;
    const int pad = 4;
    int line = g_app.scroll + (py - e.y - pad) / kLineH;
    if (line < 0) line = 0;
    if (line >= g_app.doc.line_count()) line = g_app.doc.line_count() - 1;
    int col = g_app.canvas.col_at_x(g_app.doc.line(line).c_str(),
                                    px - e.x - pad);
    return {line, col};
}

void paint_content(Canvas &cv, const ChromeLayout &lay) {
    Rect c = lay.client;
    const Theme *theme = active_theme();
    UiColors uc = ui_colors(theme);

    // Menu bar.
    Rect menu{c.x, c.y, c.w, kMenuH};
    raised_bar(cv, menu, uc);
    int x = menu.x + 8;
    for (int i = 0; i < 5; ++i) {
        int tw = cv.text_width(kMenus[i]);
        g_app.menu_rects[i] = {x, menu.y, tw + 12, kMenuH - 2};
        if (g_app.open_menu == i) cv.fill(g_app.menu_rects[i], uc.hilite);
        cv.text(x + 6, menu.y + 2, kMenus[i],
                g_app.open_menu == i ? uc.hilite_text : uc.text);
        x += tw + 12;
    }
    // Save-state indicator triangle at the right of the menu bar.
    int tx = menu.right() - 16;
    for (int i = 0; i < 6; ++i)
        cv.hline(tx - i, tx + i + 1, menu.y + 4 + i, kGlyphGrey);

    // Tab strip with one active red tab (measured: 33px strip, tab 4px
    // below its top, 24px tall).
    Rect strip{c.x, menu.bottom(), c.w, kTabH};
    raised_bar(cv, strip, uc);
    Rect tab{strip.x + 4, strip.y + 4, 108, 24};
    // Active tab plate: the theme's column-header art when present, its
    // hilite colors when themed, the Standard red bevel otherwise.
    const ThemeImage *tab_art =
        theme ? theme->image(SlotColumnHeaderHilited) : nullptr;
    if (!tab_art && theme) tab_art = theme->image(SlotColumnHeaderNormal);
    Color tab_text = kWhite;
    if (tab_art) {
        cv.nine_slice(*tab_art, tab);
        tab_text = uc.tab_text;
    } else if (theme && theme->has_colors) {
        cv.fill(tab, uc.tab);
        cv.frame(tab, from_u32(theme->color(ColColumnHeaderFrame)));
        cv.hline(tab.x + 1, tab.right() - 1, tab.y + 1, uc.tab_light);
        cv.vline(tab.x + 1, tab.y + 1, tab.bottom() - 1, uc.tab_light);
        cv.hline(tab.x + 1, tab.right() - 1, tab.bottom() - 2, uc.tab_dark);
        cv.vline(tab.right() - 2, tab.y + 1, tab.bottom() - 1, uc.tab_dark);
        tab_text = uc.tab_text;
    } else {
        bevel_box(cv, tab, false);
    }
    // Save-state square icon, then the document name.
    cv.fill({tab.x + 8, tab.y + 7, 10, 10}, Color{186, 118, 118});
    cv.frame({tab.x + 8, tab.y + 7, 10, 10}, kDeep);
    cv.text(tab.x + 24, tab.y + 4, "Untitled", tab_text);

    // Editor area: the document, with selection highlight and caret.
    Rect editor{c.x, strip.bottom(), c.w - kScrollbar,
                c.bottom() - strip.bottom()};
    g_app.editor = editor;
    cv.fill({c.x, strip.bottom(), c.w, c.bottom() - strip.bottom()},
            uc.editor_bg);
    const TextDoc &doc = g_app.doc;
    g_app.total_lines = doc.line_count();
    g_app.page_lines = (editor.h - 8) / kLineH;
    if (g_app.page_lines < 1) g_app.page_lines = 1;
    set_scroll(g_app.scroll);
    const int pad = 4;
    bool sel = doc.has_selection();
    Pos lo = doc.sel_lo(), hi = doc.sel_hi();
    int y = editor.y + pad;
    for (int i = g_app.scroll;
         i < g_app.total_lines && y + kFontHeight <= editor.bottom() - 2;
         ++i) {
        const std::string &ln = doc.line(i);
        // Selection band for this line.
        if (sel && i >= lo.line && i <= hi.line) {
            int c0 = (i == lo.line) ? lo.col : 0;
            int c1 = (i == hi.line) ? hi.col : int(ln.size());
            int sx = editor.x + pad + cv.text_width_n(ln.c_str(), c0);
            int sw = cv.text_width_n(ln.c_str() + c0, c1 - c0);
            if (i < hi.line) sw += 4; // show the trailing newline
            cv.fill({sx, y - 1, sw, kLineH}, uc.hilite);
        }
        cv.text(editor.x + pad, y, ln.c_str(), uc.editor_fg);
        y += kLineH;
    }
    // Caret: a blinking vertical bar at the caret column when focused.
    Pos car = doc.caret();
    if (g_app.focused && g_app.caret_on && car.line >= g_app.scroll &&
        car.line < g_app.scroll + g_app.page_lines) {
        int cx = editor.x + pad +
                 cv.text_width_n(doc.line(car.line).c_str(), car.col);
        int cy = editor.y + pad + (car.line - g_app.scroll) * kLineH;
        cv.vline(cx, cy - 1, cy + kFontHeight + 1, uc.editor_fg);
    }

    // Vertical scrollbar, stopping above the grow box.
    Rect sb{c.right() - kScrollbar + 1, editor.y, kScrollbar,
            g_app.lay.grip.y - editor.y};
    g_app.sb = sb;
    // Scrollbar body: the theme's double-arrows art is the whole bar
    // (arrow pairs baked into its caps, travel bounds in its positions).
    const ThemeImage *bar_art =
        theme ? theme->image(SlotVScrollDoubleArrows) : nullptr;
    if (bar_art) {
        cv.nine_slice(*bar_art, sb);
        int top_zone = bar_art->positions[1] > 0 ? bar_art->positions[1]
                                                 : bar_art->caps[1];
        int bot_zone = bar_art->positions[3] > 0 ? bar_art->positions[3]
                                                 : bar_art->caps[3];
        int half_t = top_zone / 2, half_b = bot_zone / 2;
        g_app.up1 = {sb.x, sb.y, sb.w, half_t};
        g_app.dn1 = {sb.x, sb.y + half_t, sb.w, top_zone - half_t};
        g_app.up2 = {sb.x, sb.bottom() - bot_zone, sb.w, half_b};
        g_app.dn2 = {sb.x, sb.bottom() - bot_zone + half_b, sb.w,
                     bot_zone - half_b};
    } else {
        cv.fill(sb, uc.track);
        cv.frame(sb, kBlack);
        // Arrow boxes: a dec+inc pair at each end.
        auto arrow_box = [&](int y0, int h, bool up) {
            Rect b{sb.x, y0, sb.w, h};
            cv.fill(b, uc.thumb);
            cv.frame(b, kBlack);
            cv.hline(b.x + 1, b.right() - 1, b.y + 1, uc.thumb_hi);
            cv.vline(b.x + 1, b.y + 1, b.bottom() - 1, uc.thumb_hi);
            int cx = b.x + b.w / 2, cy = b.y + b.h / 2;
            for (int i = 0; i < 4; ++i) {
                int w = up ? i : 3 - i;
                cv.hline(cx - w, cx + w + 1, cy - 2 + i, uc.text);
            }
            return b;
        };
        int ah = 13;
        g_app.up1 = arrow_box(sb.y, ah, true);
        g_app.dn1 = arrow_box(sb.y + ah - 1, ah, false);
        g_app.up2 = arrow_box(sb.bottom() - 2 * ah + 1, ah, true);
        g_app.dn2 = arrow_box(sb.bottom() - ah, ah, false);
    }
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
    // Thumb: the theme's scroll indicator art when present, primitive
    // otherwise.
    const ThemeImage *ind =
        theme ? theme->image(SlotVScrollIndicatorNormal) : nullptr;
    if (ind) {
        cv.nine_slice(*ind, thumb);
    } else {
        cv.fill(thumb, uc.thumb);
        cv.frame(thumb, kBlack);
        cv.hline(thumb.x + 1, thumb.right() - 1, thumb.y + 1, uc.thumb_hi);
        cv.vline(thumb.x + 1, thumb.y + 1, thumb.bottom() - 1, uc.thumb_hi);
        if (th >= 14)
            for (int i = 0; i < 3; ++i)
                cv.hline(thumb.x + 4, thumb.right() - 4,
                         thumb.y + th / 2 - 3 + i * 3, uc.bar_dark);
    }
}

int menu_item_count(int m) {
    if (m == 4) return 1 + int(g_app.hap_names.size());
    int n = 0;
    while (n < 6 && kMenuItems[m][n]) ++n;
    return n;
}

const char *menu_item_text(int m, int i) {
    if (m == 4)
        return i == 0 ? "Haxial Standard" : g_app.hap_names[i - 1].c_str();
    return kMenuItems[m][i];
}

// Dropdown item index at a point, honoring the column layout.
int dropdown_index(int x, int y) {
    if (!g_app.dropdown.contains(x, y)) return -1;
    int col = (x - g_app.dropdown.x - 2) / g_app.dd_colw + g_app.dd_scroll;
    int row = (y - g_app.dropdown.y - 2) / g_app.item_h;
    if (row < 0 || row >= g_app.dd_rows) return -1;
    if (col < 0 || col >= g_app.dd_cols_total) return -1;
    return col * g_app.dd_rows + row;
}

// Clamp the dropdown column-scroll to the valid range.
void clamp_dd_scroll() {
    int max_scroll = g_app.dd_cols_total - g_app.dd_cols_shown;
    if (max_scroll < 0) max_scroll = 0;
    if (g_app.dd_scroll > max_scroll) g_app.dd_scroll = max_scroll;
    if (g_app.dd_scroll < 0) g_app.dd_scroll = 0;
}

// The open pull-down menu: dark raised panel, red hilite bar, painted last.
// Long menus (Appearance with many .haps) wrap into multiple columns.
void paint_dropdown(Canvas &cv) {
    int m = g_app.open_menu;
    if (m < 0) return;
    int n = menu_item_count(m);
    int wmax = 0;
    for (int i = 0; i < n; ++i) {
        int tw = cv.text_width(menu_item_text(m, i));
        if (tw > wmax) wmax = tw;
    }
    int top = g_app.menu_rects[m].bottom() + 2;
    int max_rows = (cv.height() - top - 8) / g_app.item_h;
    if (max_rows < 1) max_rows = 1;
    int rows = n < max_rows ? n : max_rows;
    int cols = (n + rows - 1) / rows;
    int colw = wmax + 24;
    g_app.dd_rows = rows;
    g_app.dd_colw = colw;
    g_app.dd_cols_total = cols;

    // How many columns fit the window width; the rest scroll into view.
    int cols_fit = (cv.width() - 6) / colw;
    if (cols_fit < 1) cols_fit = 1;
    int cols_shown = cols < cols_fit ? cols : cols_fit;
    g_app.dd_cols_shown = cols_shown;
    clamp_dd_scroll();
    bool can_left = g_app.dd_scroll > 0;
    bool can_right = g_app.dd_scroll + cols_shown < cols;
    int arrow_h = (can_left || can_right) ? g_app.item_h : 0;

    int rx = g_app.menu_rects[m].x;
    if (rx + cols_shown * colw + 4 > cv.width())
        rx = cv.width() - cols_shown * colw - 4;
    if (rx < 0) rx = 0;
    Rect r{rx, top, cols_shown * colw + 4,
           rows * g_app.item_h + 4 + arrow_h};
    g_app.dropdown = r;
    UiColors uc = ui_colors(active_theme());
    cv.fill(r, uc.bar_body);
    cv.frame(r, kBlack);
    cv.hline(r.x + 1, r.right() - 1, r.y + 1, uc.bar_light);
    cv.vline(r.x + 1, r.y + 1, r.bottom() - 1, uc.bar_light);
    cv.hline(r.x + 1, r.right() - 1, r.bottom() - 2, uc.bar_dark);
    cv.vline(r.right() - 2, r.y + 1, r.bottom() - 1, uc.bar_dark);
    for (int i = 0; i < n; ++i) {
        int col = i / rows, row = i % rows;
        if (col < g_app.dd_scroll || col >= g_app.dd_scroll + cols_shown)
            continue;
        Rect item{r.x + 2 + (col - g_app.dd_scroll) * colw,
                  r.y + 2 + row * g_app.item_h, colw, g_app.item_h};
        bool hot = i == g_app.hot_item;
        if (hot) cv.fill(item, uc.hilite);
        Color fg = hot ? uc.hilite_text : uc.text;
        if (m == 4 && i == g_app.theme_index)
            cv.text(item.x + 2, item.y + 1, ">", fg);
        cv.text(item.x + 10, item.y + 1, menu_item_text(m, i), fg);
    }
    // Scroll affordances: "<< more" / "more >>" row along the bottom.
    if (arrow_h) {
        int ay = r.bottom() - arrow_h - 1;
        cv.hline(r.x + 1, r.right() - 1, ay, uc.bar_dark);
        if (can_left) cv.text(r.x + 8, ay + 2, "<< more", uc.text);
        if (can_right) {
            const char *t = "more >>";
            cv.text(r.right() - cv.text_width(t) - 8, ay + 2, t, uc.text);
        }
    }
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

    paint_chrome(cv, g_app.lay, "TE: Untitled", g_app.focused, g_app.hot_box,
                 g_app.pressed_box, theme);
    paint_content(cv, g_app.lay);
    paint_grip(cv, g_app.lay.grip, g_app.focused, theme);
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
                    // Bottom "<< more / more >>" row: scroll a column.
                    int arrow_top =
                        g_app.dropdown.bottom() - g_app.item_h - 1;
                    if (g_app.dd_cols_total > g_app.dd_cols_shown &&
                        y >= arrow_top) {
                        int mid = g_app.dropdown.x + g_app.dropdown.w / 2;
                        g_app.dd_scroll += (x < mid ? -1 : 1);
                        clamp_dd_scroll();
                        InvalidateRect(hwnd, nullptr, FALSE);
                        return 0;
                    }
                    int i = dropdown_index(x, y);
                    int m = g_app.open_menu;
                    g_app.open_menu = -1;
                    if (i >= 0 && i < menu_item_count(m)) {
                        if (m == 0 && (i == 4 || i == 5))
                            DestroyWindow(hwnd); // Close / Quit
                        else if (m == 4)
                            select_theme(i);
                    }
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
                    g_app.dd_scroll = 0;
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
            // Click in the editor: place the caret, begin selection.
            if (g_app.editor.contains(x, y)) {
                bool extend = GetKeyState(VK_SHIFT) < 0;
                g_app.doc.set_caret(pos_at(x, y), extend);
                g_app.selecting = true;
                wake_caret();
                SetCapture(hwnd);
                SetFocus(hwnd);
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
            } else if (y < g_app.lay.title_h) {
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
            if (g_app.selecting) {
                g_app.doc.set_caret(pos_at(x, y), true);
                ensure_caret_visible();
                wake_caret();
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (g_app.open_menu >= 0) {
                int hot = dropdown_index(x, y);
                if (hot != g_app.hot_item) {
                    g_app.hot_item = hot;
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
            }
            return 0;
        }
        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;
            if (g_app.open_menu >= 0) {
                // Scroll the open menu's columns instead of the document.
                g_app.dd_scroll -= delta;
                clamp_dd_scroll();
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            set_scroll(g_app.scroll - delta * 3);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_KEYDOWN: {
            if (wp == VK_ESCAPE && g_app.open_menu >= 0) {
                g_app.open_menu = -1;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            bool shift = GetKeyState(VK_SHIFT) < 0;
            bool ctrl = GetKeyState(VK_CONTROL) < 0;
            TextDoc &d = g_app.doc;
            switch (wp) {
                case VK_LEFT: d.move_left(shift); break;
                case VK_RIGHT: d.move_right(shift); break;
                case VK_UP: d.move_up(shift); break;
                case VK_DOWN: d.move_down(shift); break;
                case VK_HOME: d.move_home(shift); break;
                case VK_END: d.move_end(shift); break;
                case VK_PRIOR: // Page Up
                    for (int i = 0; i < g_app.page_lines; ++i) d.move_up(shift);
                    break;
                case VK_NEXT: // Page Down
                    for (int i = 0; i < g_app.page_lines; ++i)
                        d.move_down(shift);
                    break;
                case VK_BACK: d.backspace(); break;
                case VK_DELETE: d.del_forward(); break;
                case 'A':
                    if (ctrl) d.select_all(); else return 0;
                    break;
                case 'C':
                    if (ctrl) clipboard_copy(hwnd, d.selected_text());
                    else return 0;
                    break;
                case 'X':
                    if (ctrl) {
                        clipboard_copy(hwnd, d.selected_text());
                        d.insert_text("");
                    } else return 0;
                    break;
                case 'V':
                    if (ctrl) d.insert_text(clipboard_paste(hwnd));
                    else return 0;
                    break;
                default:
                    return 0;
            }
            ensure_caret_visible();
            wake_caret();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_CHAR: {
            char ch = char(wp);
            if (ch == '\r') {
                g_app.doc.newline();
            } else if (ch == '\t') {
                g_app.doc.type_char(' ');
                g_app.doc.type_char(' ');
                g_app.doc.type_char(' ');
                g_app.doc.type_char(' ');
            } else if ((unsigned char)ch >= 32 && (unsigned char)ch < 127) {
                g_app.doc.type_char(ch);
            } else {
                return 0;
            }
            ensure_caret_visible();
            wake_caret();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_LBUTTONUP: {
            if (g_app.selecting) {
                g_app.selecting = false;
                ReleaseCapture();
                return 0;
            }
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
            // Blink the caret (~500ms) while focused and not selecting.
            static int tick = 0;
            if (++tick % 2 == 0 && g_app.focused && !g_app.selecting) {
                g_app.caret_on = !g_app.caret_on;
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
    discover_haps();
    g_app.doc.set_text(
        "'Twas brillig, and the slithy toves\n"
        "Did gyre and gimble in the wabe;\n"
        "All mimsy were the borogoves,\n"
        "And the mome raths outgrabe.\n"
        "\n"
        "Type here -- this is a real editor now: click to place the "
        "caret,\n"
        "drag to select, and the usual keys work (arrows, Home/End,\n"
        "PageUp/Down, Backspace/Delete, Enter, Ctrl+A/C/X/V).");

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
