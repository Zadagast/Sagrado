// Persist top-level window positions (and sizes when resizable) next to the
// executable as windows.txt. Shared by KDX and TextEdit — popups, menus and
// pickers are transient and are not saved.
#pragma once

#include <stdio.h>
#include <string.h>
#include <windows.h>

#include <string>
#include <vector>

namespace winpos {

struct Entry {
    char name[32]{};
    int x = 0, y = 0, w = 0, h = 0;
};

inline std::vector<Entry> g_entries;
inline bool g_loaded = false;

inline std::string exe_dir() {
    char exe[MAX_PATH];
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    std::string dir(exe);
    size_t cut = dir.find_last_of("\\/");
    return cut == std::string::npos ? std::string(".") : dir.substr(0, cut);
}

inline std::string path() { return exe_dir() + "\\windows.txt"; }

inline Entry *find(const char *name) {
    for (Entry &e : g_entries)
        if (!strcmp(e.name, name)) return &e;
    return nullptr;
}

inline void load() {
    if (g_loaded) return;
    g_loaded = true;
    g_entries.clear();
    FILE *f = fopen(path().c_str(), "rb");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *nl = line + strlen(line);
        while (nl > line && (nl[-1] == '\n' || nl[-1] == '\r')) *--nl = 0;
        char name[32];
        int x, y, w, h;
        if (sscanf(line, "%31[^=]=%d,%d,%d,%d", name, &x, &y, &w, &h) == 5) {
            Entry e{};
            strncpy(e.name, name, sizeof(e.name) - 1);
            e.x = x;
            e.y = y;
            e.w = w;
            e.h = h;
            g_entries.push_back(e);
        }
    }
    fclose(f);
}

inline void save() {
    FILE *f = fopen(path().c_str(), "wb");
    if (!f) return;
    for (const Entry &e : g_entries)
        fprintf(f, "%s=%d,%d,%d,%d\n", e.name, e.x, e.y, e.w, e.h);
    fclose(f);
}

// Keep at least 40×40 of the window on the work area so a changed monitor
// layout cannot strand it off-screen.
inline void clamp(int &x, int &y, int w, int h) {
    RECT wa{};
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &wa, 0);
    if (w < 40) w = 40;
    if (h < 40) h = 40;
    if (x + w < wa.left + 40) x = wa.left;
    if (y + h < wa.top + 40) y = wa.top;
    if (x > wa.right - 40) x = wa.right - w;
    if (y > wa.bottom - 40) y = wa.bottom - h;
}

// Resolve create coords. When `size` is false, only x/y come from the store
// (dialogs with fixed design sizes). When true, w/h are restored too.
inline void resolve(const char *name, int &x, int &y, int &w, int &h,
                    bool size) {
    load();
    if (const Entry *e = find(name)) {
        x = e->x;
        y = e->y;
        if (size && e->w > 0 && e->h > 0) {
            w = e->w;
            h = e->h;
        }
    }
    clamp(x, y, w, h);
}

inline void put(const char *name, int x, int y, int w, int h) {
    load();
    clamp(x, y, w, h);
    if (Entry *e = find(name)) {
        e->x = x;
        e->y = y;
        e->w = w;
        e->h = h;
    } else {
        Entry neu{};
        strncpy(neu.name, name, sizeof(neu.name) - 1);
        neu.x = x;
        neu.y = y;
        neu.w = w;
        neu.h = h;
        g_entries.push_back(neu);
    }
    save();
}

// Snapshot a live HWND. When `size` is false, keep the previously stored
// width/height (or the window's current size if first save).
inline void remember(HWND hwnd, const char *name, bool size = true) {
    if (!hwnd || !IsWindow(hwnd) || !IsWindowVisible(hwnd)) return;
    RECT rc{};
    if (!GetWindowRect(hwnd, &rc)) return;
    int x = rc.left, y = rc.top, w = rc.right - rc.left, h = rc.bottom - rc.top;
    if (!size) {
        load();
        if (const Entry *e = find(name)) {
            if (e->w > 0) w = e->w;
            if (e->h > 0) h = e->h;
        }
    }
    put(name, x, y, w, h);
}

} // namespace winpos
