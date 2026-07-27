// KDX Settings: Identity + Appearance pages, modeled on the real Haxial
// Settings window. Cancel / Apply / Save along the top with a category
// dropdown; identity (nick, colours, icon) and appearance (.hap theme)
// persist next to the executable as settings.txt.
#pragma once

#include <stdio.h>

#include <string>
#include <vector>

#include "canvas.h"
#include "chrome.h"
#include "controls.h"
#include "hap.h"
#include "menu.h"
#include "room.h"

namespace settings {

constexpr int kW = 420, kH = 340;
constexpr UINT WM_SETTINGS_PICK = WM_APP + 5;

enum Page { PageIdentity = 0, PageAppearance = 1 };
enum Focus {
    FocusNone = -1,
    FocusName = 0,
    FocusDesc = 1,
};

struct Prefs {
    std::string nick = "New KDX User";
    std::string description;
    uint32_t fg = 0x00ff00, bg = 0x000000;
    std::string theme_name = "Haxial Standard";  // empty stem → Standard
    bool small_file_icons = false;
    bool small_user_icons = false;
    bool small_button_bar = false;
};

struct ThemeState {
    Theme theme;
    bool themed = false;
    int index = 0;  // 0 = Standard
    std::vector<std::string> hap_names, hap_paths;
};

inline Prefs g_prefs;       // applied
inline Prefs g_draft;       // being edited
inline ThemeState g_theme;
inline Page g_page = PageIdentity;

inline std::string exe_dir() {
    char exe[MAX_PATH];
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    std::string dir(exe);
    size_t cut = dir.find_last_of("\\/");
    return cut == std::string::npos ? std::string(".") : dir.substr(0, cut);
}

inline std::string prefs_path() { return exe_dir() + "\\settings.txt"; }

inline void discover_haps() {
    g_theme.hap_names.clear();
    g_theme.hap_paths.clear();
    std::string dir = exe_dir();
    const char *cands[] = {
        "\\Appearances",
        "\\themes\\Appearances",
        "\\..\\themes\\Appearances",
        "\\..\\..\\themes\\Appearances",
        "\\..\\..\\..\\themes\\Appearances",
    };
    for (const char *c : cands) {
        std::string base = dir + c;
        WIN32_FIND_DATAA fd;
        HANDLE fh = FindFirstFileA((base + "\\*.hap").c_str(), &fd);
        if (fh == INVALID_HANDLE_VALUE) continue;
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            std::string name(fd.cFileName);
            if (name.size() < 5) continue;
            g_theme.hap_paths.push_back(base + "\\" + name);
            g_theme.hap_names.push_back(name.substr(0, name.size() - 4));
        } while (FindNextFileA(fh, &fd));
        FindClose(fh);
        if (!g_theme.hap_paths.empty()) break;
    }
}

// Load a theme into the live renderer only. Does not touch g_prefs — that
// is Apply/Save's job, so closing Settings without Apply can revert.
inline bool select_theme_by_index(int index) {
    g_theme.index = index;
    g_theme.themed = false;
    if (index > 0 && index <= int(g_theme.hap_paths.size())) {
        Theme t;
        if (load_hap(g_theme.hap_paths[index - 1], t)) {
            g_theme.theme = std::move(t);
            g_theme.themed = true;
            return true;
        }
        g_theme.index = 0;
        return false;
    }
    return index == 0;
}

inline bool select_theme_by_name(const std::string &name) {
    if (name.empty() || name == "Haxial Standard")
        return select_theme_by_index(0);
    for (size_t i = 0; i < g_theme.hap_names.size(); ++i)
        if (g_theme.hap_names[i] == name)
            return select_theme_by_index(int(i) + 1);
    return select_theme_by_index(0);
}

inline const Theme *active_theme() {
    return g_theme.themed ? &g_theme.theme : nullptr;
}

inline const char *theme_label() {
    if (g_theme.index <= 0 || g_theme.index > int(g_theme.hap_names.size()))
        return "Haxial Standard";
    return g_theme.hap_names[g_theme.index - 1].c_str();
}

inline const char *draft_theme_label() {
    if (g_draft.theme_name.empty() || g_draft.theme_name == "Haxial Standard")
        return "Haxial Standard";
    return g_draft.theme_name.c_str();
}

inline void apply_identity_to_session() {
    room::g_pref_nick = g_prefs.nick;
    room::g_pref_fg = g_prefs.fg;
    room::g_pref_bg = g_prefs.bg;
    if (room::on_server()) {
        {
            room::Guard lk(&room::g.lock);
            room::g.me.nick = g_prefs.nick.empty() ? "New KDX User" : g_prefs.nick;
            room::g.me.fg = g_prefs.fg;
            room::g.me.bg = g_prefs.bg;
        }
        room::refresh_users();
        if (room::g.role == room::Host && room::g.connected)
            room::send_to(0, room::user_list_payload());
    }
}

inline bool load_prefs() {
    FILE *f = fopen(prefs_path().c_str(), "rb");
    if (!f) return false;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *nl = line + strlen(line);
        while (nl > line && (nl[-1] == '\n' || nl[-1] == '\r')) *--nl = 0;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        const char *k = line, *v = eq + 1;
        if (!strcmp(k, "nick")) g_prefs.nick = v;
        else if (!strcmp(k, "description")) g_prefs.description = v;
        else if (!strcmp(k, "fg")) g_prefs.fg = strtoul(v, nullptr, 16);
        else if (!strcmp(k, "bg")) g_prefs.bg = strtoul(v, nullptr, 16);
        else if (!strcmp(k, "theme")) g_prefs.theme_name = v;
        else if (!strcmp(k, "small_file_icons"))
            g_prefs.small_file_icons = v[0] == '1';
        else if (!strcmp(k, "small_user_icons"))
            g_prefs.small_user_icons = v[0] == '1';
        else if (!strcmp(k, "small_button_bar"))
            g_prefs.small_button_bar = v[0] == '1';
    }
    fclose(f);
    return true;
}

inline bool save_prefs() {
    FILE *f = fopen(prefs_path().c_str(), "wb");
    if (!f) return false;
    std::string desc = g_prefs.description;
    for (char &c : desc)
        if (c == '\n') c = ' ';
    fprintf(f, "nick=%s\n", g_prefs.nick.c_str());
    fprintf(f, "description=%s\n", desc.c_str());
    fprintf(f, "fg=%06X\n", g_prefs.fg & 0xffffff);
    fprintf(f, "bg=%06X\n", g_prefs.bg & 0xffffff);
    fprintf(f, "theme=%s\n", g_prefs.theme_name.c_str());
    fprintf(f, "small_file_icons=%d\n", g_prefs.small_file_icons ? 1 : 0);
    fprintf(f, "small_user_icons=%d\n", g_prefs.small_user_icons ? 1 : 0);
    fprintf(f, "small_button_bar=%d\n", g_prefs.small_button_bar ? 1 : 0);
    fclose(f);
    return true;
}

inline void boot() {
    discover_haps();
    load_prefs();
    if (g_prefs.nick.empty()) g_prefs.nick = "New KDX User";
    select_theme_by_name(g_prefs.theme_name);
    apply_identity_to_session();
    g_draft = g_prefs;
}

// ---- window ----

struct Window {
    HWND hwnd = nullptr;
    Canvas canvas;
    ChromeLayout lay{};
    int pressed_box = 0;
    int pressed_btn = -1;  // 0 cat, 1 cancel, 2 apply, 3 save, 4–7 page
    int focus = FocusName;
    bool caret = true;
    int color_target = 0;  // 0 fg, 1 bg when picking
    Rect cat{}, cancel{}, apply{}, save{};
    Rect name{}, desc{}, fg_sw{}, bg_sw{}, icon{};
    Rect appearance{}, chk_file{}, chk_user{}, chk_bar{};
    Rect font_title{}, font_chat{}, font_files{}, font_users{};
};

inline Window g;
inline void (*invalidate_all)() = nullptr;

inline void request_redraw_all() {
    if (g.hwnd) {
        InvalidateRect(g.hwnd, nullptr, FALSE);
        UpdateWindow(g.hwnd);
    }
    if (invalidate_all) invalidate_all();
}

inline void layout(int w, int h) {
    g.lay = chrome_layout(w, h, active_theme(), GetForegroundWindow() == g.hwnd);
    g.lay.max_box = {0, 0, 0, 0};
    g.lay.grip = {0, 0, 0, 0};
    g.lay.hatch_box = {0, 0, 0, 0};
    Rect cl = g.lay.client;
    int y = cl.y + 8;
    g.cat = {cl.x + 10, y, 120, 24};
    g.save = {cl.right() - 10 - 72, y, 72, 24};
    g.apply = {g.save.x - 8 - 72, y, 72, 24};
    g.cancel = {g.apply.x - 8 - 72, y, 72, 24};
    y = g.cat.bottom() + 14;
    int lx = cl.x + 14, fx = lx + 100;
    int fw = cl.right() - 14 - fx;
    g.name = {fx, y, fw, 20};
    y += 28;
    g.desc = {fx, y, fw, 72};
    y = g.desc.bottom() + 14;
    g.fg_sw = {lx + 130, y, 36, 20};
    g.bg_sw = {lx + 310, y, 36, 20};
    y += 36;
    g.icon = {fx, y, 48, 48};

    y = g.cat.bottom() + 14;
    g.appearance = {fx, y, fw, 24};
    y += 36;
    g.chk_file = {lx, y, 280, 16};
    y += 22;
    g.chk_user = {lx, y, 280, 16};
    y += 22;
    g.chk_bar = {lx, y, 280, 16};
    y += 30;
    int bw = fw;
    g.font_title = {fx, y, bw, 24};
    y += 28;
    g.font_chat = {fx, y, bw, 24};
    y += 28;
    g.font_files = {fx, y, bw, 24};
    y += 28;
    g.font_users = {fx, y, bw, 24};
}

inline void draw_swatch(Canvas &cv, Rect r, uint32_t rgb, bool hot,
                        const DialogColors &dc) {
    Color c = from_u32(rgb);
    cv.fill(r, c);
    cv.frame(r, hot ? dc.field_focus : dc.btn_frame);
    if (hot) cv.frame({r.x + 1, r.y + 1, r.w - 2, r.h - 2}, dc.field_focus);
}

inline void draw_desc(Canvas &cv, Rect r, const std::string &text, bool focused,
                      bool caret_on, const DialogColors &dc) {
    cv.fill(r, dc.field_bg);
    if (focused) {
        cv.frame(r, dc.field_focus);
        cv.frame({r.x + 1, r.y + 1, r.w - 2, r.h - 2}, dc.field_focus);
    } else {
        cv.frame(r, dc.field_frame);
    }
    cv.set_clip({r.x + 3, r.y + 3, r.w - 6, r.h - 6});
    int x = r.x + 5, y = r.y + 4;
    std::string line;
    for (char ch : text) {
        if (ch == '\n') {
            cv.text(x, y, line.c_str(), dc.field_fg);
            y += kFontHeight + 2;
            line.clear();
            continue;
        }
        line += ch;
        if (cv.text_width(line.c_str()) > r.w - 14) {
            char last = line.back();
            line.pop_back();
            cv.text(x, y, line.c_str(), dc.field_fg);
            y += kFontHeight + 2;
            line = last;
        }
    }
    int end = cv.text(x, y, line.c_str(), dc.field_fg);
    if (focused && caret_on)
        cv.vline(end + 1, y, y + kFontHeight, dc.caret);
    cv.clear_clip();
}

inline void paint() {
    if (!g.hwnd) return;
    RECT rc;
    GetClientRect(g.hwnd, &rc);
    if (rc.right <= 0 || rc.bottom <= 0) return;
    if (g.canvas.width() != rc.right || g.canvas.height() != rc.bottom)
        g.canvas.resize(rc.right, rc.bottom);
    bool focused = GetForegroundWindow() == g.hwnd;
    layout(rc.right, rc.bottom);
    const Theme *theme = active_theme();
    paint_chrome(g.canvas, g.lay, "KDX Settings", focused, 0,
                 g.pressed_box == 5 ? 1 : 0, theme);
    DialogColors dc = dialog_colors(theme);
    g.canvas.fill(g.lay.client, dc.workspace);

    const char *cat = g_page == PageIdentity ? "Identity" : "Appearance";
    draw_button(g.canvas, g.cat, cat, g.pressed_btn == 0, false, dc);
    // dropdown chevron
    {
        Rect r = g.cat;
        int cx = r.right() - 12, cy = r.y + r.h / 2;
        Color mark = dc.btn_label;
        for (int i = 0; i < 3; ++i) {
            g.canvas.hline(cx - 3 + i, cx + 3 - i, cy - 1 + i, mark);
        }
    }
    draw_button(g.canvas, g.cancel, "Cancel", g.pressed_btn == 1, false, dc);
    draw_button(g.canvas, g.apply, "Apply", g.pressed_btn == 2, false, dc);
    draw_button(g.canvas, g.save, "Save", g.pressed_btn == 3, true, dc);

    int lx = g.lay.client.x + 14;
    if (g_page == PageIdentity) {
        g.canvas.text(lx, g.name.y + 4, "Name:", dc.label);
        draw_field(g.canvas, g.name, g_draft.nick.c_str(),
                   focused && g.focus == FocusName, g.caret, dc);
        g.canvas.text(lx, g.desc.y + 4, "Description:", dc.label);
        draw_desc(g.canvas, g.desc, g_draft.description,
                  focused && g.focus == FocusDesc, g.caret, dc);
        g.canvas.text(lx, g.fg_sw.y + 4, "Foreground Color:", dc.label);
        draw_swatch(g.canvas, g.fg_sw, g_draft.fg, g.pressed_btn == 4, dc);
        g.canvas.text(g.fg_sw.right() + 16, g.bg_sw.y + 4, "Background Color:",
                      dc.label);
        draw_swatch(g.canvas, g.bg_sw, g_draft.bg, g.pressed_btn == 5, dc);
        g.canvas.text(lx, g.icon.y - 16, "Icon:", dc.label);
        g.canvas.fill(g.icon, kBlack);
        g.canvas.frame(g.icon, dc.btn_frame);
        // Placeholder icon: filled with the user's colours + a simple mark.
        g.canvas.fill({g.icon.x + 4, g.icon.y + 4, g.icon.w - 8, g.icon.h - 8},
                      from_u32(g_draft.bg));
        g.canvas.text(g.icon.x + (g.icon.w - g.canvas.text_width("KDX")) / 2,
                      g.icon.y + (g.icon.h - kFontHeight) / 2, "KDX",
                      from_u32(g_draft.fg));
    } else {
        g.canvas.text(lx, g.appearance.y + 6, "Appearance:", dc.label);
        draw_button(g.canvas, g.appearance, draft_theme_label(),
                    g.pressed_btn == 4, false, dc);
        {
            Rect r = g.appearance;
            int cx = r.right() - 12, cy = r.y + r.h / 2;
            for (int i = 0; i < 3; ++i)
                g.canvas.hline(cx - 3 + i, cx + 3 - i, cy - 1 + i, dc.btn_label);
        }
        draw_checkbox(g.canvas, g.chk_file.x, g.chk_file.y,
                      g_draft.small_file_icons,
                      "Use Small Icons in File Lists", dc);
        draw_checkbox(g.canvas, g.chk_user.x, g.chk_user.y,
                      g_draft.small_user_icons,
                      "Use Small Icons in User Lists", dc);
        draw_checkbox(g.canvas, g.chk_bar.x, g.chk_bar.y,
                      g_draft.small_button_bar, "Small Button Bar", dc);

        auto font_row = [&](Rect r, const char *label, int press_id) {
            g.canvas.text(lx, r.y + 6, label, dc.label);
            draw_button(g.canvas, r, "Pixel Operator Bold, 12",
                        g.pressed_btn == press_id, false, dc);
        };
        font_row(g.font_title, "Title Font:", 5);
        font_row(g.font_chat, "Chat/News Font:", 6);
        font_row(g.font_files, "File List Font:", 7);
        font_row(g.font_users, "User List Font:", 8);
    }
}

inline void apply_draft(bool write_disk) {
    g_prefs = g_draft;
    if (!select_theme_by_name(g_prefs.theme_name)) {
        g_prefs.theme_name = "Haxial Standard";
        g_draft.theme_name = g_prefs.theme_name;
        select_theme_by_index(0);
    }
    apply_identity_to_session();
    if (write_disk) save_prefs();
    request_redraw_all();
}

// Live preview: skins the app as soon as a .hap is chosen. g_prefs is
// untouched, so Cancel / close without Apply puts the previous theme back.
inline void preview_draft_theme() {
    select_theme_by_name(g_draft.theme_name);
    request_redraw_all();
}

inline void discard_unapplied() {
    g_draft = g_prefs;
    select_theme_by_name(g_prefs.theme_name);
    request_redraw_all();
}

inline void cancel_and_close() {
    discard_unapplied();
    if (g.hwnd) DestroyWindow(g.hwnd);
}

// ---- colour / appearance pickers (scrollable list popup) ----

struct Picker {
    HWND hwnd = nullptr;
    Canvas canvas;
    std::vector<std::string> labels;
    std::vector<uint32_t> colors;  // empty → text list
    int scroll = 0;
    int hot = -1;
    int kind = 0;  // 0 appearance, 1 color
};

inline Picker g_pick;
constexpr int kPickRow = 18;
constexpr int kPickMaxRows = 16;

inline void paint_picker() {
    if (!g_pick.hwnd) return;
    RECT rc;
    GetClientRect(g_pick.hwnd, &rc);
    int w = rc.right, h = rc.bottom;
    if (g_pick.canvas.width() != w || g_pick.canvas.height() != h)
        g_pick.canvas.resize(w, h);
    DialogColors dc = dialog_colors(active_theme());
    ListColors lc = list_colors(active_theme());
    g_pick.canvas.fill({0, 0, w, h}, dc.workspace);
    g_pick.canvas.frame({0, 0, w, h}, dc.field_focus);
    int rows = (h - 4) / kPickRow;
    int n = int(g_pick.labels.size());
    for (int i = 0; i < rows; ++i) {
        int idx = g_pick.scroll + i;
        if (idx >= n) break;
        Rect row{2, 2 + i * kPickRow, w - 4, kPickRow};
        bool hot = (idx == g_pick.hot);
        if (hot) g_pick.canvas.fill(row, lc.hilite_bg);
        Color ink = hot ? lc.hilite_fg : dc.label;
        if (!g_pick.colors.empty()) {
            Rect sw{row.x + 4, row.y + 3, 20, kPickRow - 6};
            g_pick.canvas.fill(sw, from_u32(g_pick.colors[idx]));
            g_pick.canvas.frame(sw, primary_frame(active_theme()));
            g_pick.canvas.text(sw.right() + 6, row.y + 3,
                               g_pick.labels[idx].c_str(), ink);
        } else {
            g_pick.canvas.text(row.x + 6, row.y + 3, g_pick.labels[idx].c_str(),
                               ink);
        }
    }
}

inline LRESULT CALLBACK pick_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

inline void close_picker() {
    if (!g_pick.hwnd) return;
    HWND h = g_pick.hwnd;
    g_pick.hwnd = nullptr;
    DestroyWindow(h);
}

inline void open_picker(HINSTANCE hinst, int sx, int sy, int kind) {
    close_picker();
    g_pick.kind = kind;
    g_pick.scroll = 0;
    g_pick.hot = -1;
    g_pick.labels.clear();
    g_pick.colors.clear();
    if (kind == 0) {
        g_pick.labels.push_back("Haxial Standard");
        for (const auto &n : g_theme.hap_names) g_pick.labels.push_back(n);
    } else {
        static const uint32_t kPalette[] = {
            0x000000, 0xffffff, 0x00ff00, 0x00cc00, 0x009900, 0x00ffff,
            0x00ccff, 0x0066ff, 0x0000ff, 0x9900ff, 0xff00ff, 0xff0099,
            0xff0000, 0xff6600, 0xffcc00, 0xffff00, 0x996633, 0x666666,
            0x999999, 0xcccccc, 0x880000, 0x008800, 0x000088, 0x888800,
        };
        static const char *kNames[] = {
            "Black", "White", "Green", "Bright Green", "Forest", "Cyan",
            "Sky", "Azure", "Blue", "Purple", "Magenta", "Pink",
            "Red", "Orange", "Gold", "Yellow", "Brown", "Dim",
            "Grey", "Silver", "Maroon", "Dark Green", "Navy", "Olive",
        };
        for (size_t i = 0; i < sizeof(kPalette) / sizeof(kPalette[0]); ++i) {
            g_pick.colors.push_back(kPalette[i]);
            g_pick.labels.push_back(kNames[i]);
        }
    }
    int rows = int(g_pick.labels.size());
    if (rows > kPickMaxRows) rows = kPickMaxRows;
    int pw = kind == 0 ? 220 : 160;
    int ph = 4 + rows * kPickRow;
    static bool registered = false;
    if (!registered) {
        WNDCLASSA wc{};
        wc.lpfnWndProc = pick_proc;
        wc.hInstance = hinst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = "SagradoSettingsPick";
        RegisterClassA(&wc);
        registered = true;
    }
    g_pick.hwnd =
        CreateWindowExA(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, "SagradoSettingsPick",
                        "", WS_POPUP, sx, sy, pw, ph, g.hwnd, nullptr, hinst,
                        nullptr);
    ShowWindow(g_pick.hwnd, SW_SHOW);
    SetCapture(g_pick.hwnd);
}

inline LRESULT CALLBACK pick_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_NCCALCSIZE:
            if (wp) return 0;
            break;
        case WM_MOUSEMOVE: {
            int y = GET_Y_LPARAM(lp);
            int row = (y - 2) / kPickRow;
            int idx = g_pick.scroll + row;
            if (idx < 0 || idx >= int(g_pick.labels.size())) idx = -1;
            if (idx != g_pick.hot) {
                g_pick.hot = idx;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            int y = GET_Y_LPARAM(lp);
            int row = (y - 2) / kPickRow;
            int idx = g_pick.scroll + row;
            if (idx >= 0 && idx < int(g_pick.labels.size()) && g.hwnd)
                PostMessage(g.hwnd, WM_SETTINGS_PICK,
                            WPARAM(g_pick.kind), LPARAM(idx));
            close_picker();
            return 0;
        }
        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wp) > 0 ? -3 : 3;
            int max_scroll = int(g_pick.labels.size()) - kPickMaxRows;
            if (max_scroll < 0) max_scroll = 0;
            g_pick.scroll += delta;
            if (g_pick.scroll < 0) g_pick.scroll = 0;
            if (g_pick.scroll > max_scroll) g_pick.scroll = max_scroll;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_CAPTURECHANGED:
            if ((HWND)lp != hwnd) close_picker();
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            paint_picker();
            BITMAPINFO bi{};
            bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bi.bmiHeader.biWidth = g_pick.canvas.width();
            bi.bmiHeader.biHeight = -g_pick.canvas.height();
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = 32;
            bi.bmiHeader.biCompression = BI_RGB;
            SetDIBitsToDevice(hdc, 0, 0, g_pick.canvas.width(),
                              g_pick.canvas.height(), 0, 0, 0,
                              g_pick.canvas.height(), g_pick.canvas.data(), &bi,
                              DIB_RGB_COLORS);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            if (g_pick.hwnd == hwnd) g_pick.hwnd = nullptr;
            return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

inline void category_chosen(int id) {
    if (id == 1) g_page = PageIdentity;
    if (id == 2) g_page = PageAppearance;
    if (g.hwnd) InvalidateRect(g.hwnd, nullptr, FALSE);
}

inline void open_category_menu(HINSTANCE hinst) {
    std::vector<menu::Item> items{
        {"Identity", 1, true, {}, "", false},
        {"Appearance", 2, true, {}, "", false},
    };
    POINT p{g.cat.x, g.cat.bottom()};
    ClientToScreen(g.hwnd, &p);
    menu::open(hinst, g.hwnd, p.x, p.y, std::move(items), category_chosen,
               nullptr);
}

}  // namespace settings
