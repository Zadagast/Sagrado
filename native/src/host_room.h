// The Host a Server window: names a server, registers it with the Sagrado
// tracker and keeps the listing alive with heartbeats. Guests arrive over the
// tracker's relay, so there is nothing to listen on and no port to forward.
// Drawn on the Sagrado Kit like every other window — chrome, fields and
// buttons from the kit, no native controls.
#pragma once

#include <string>

#include "canvas.h"
#include "chrome.h"
#include "controls.h"
#include "tracker.h"
#include "settings.h"

namespace host_room {

using tracker::kGroupNames;

constexpr int kW = 400, kH = 204;
constexpr UINT WM_HOST_DONE = WM_APP + 2;  // the register call returned

struct Window {
    HWND hwnd = nullptr;
    Canvas canvas;
    ChromeLayout lay{};
    int pressed_box = 0;
    int focus = 0;      // 0 name, 1 description
    bool caret = true;
    int pressed_btn = -1;  // 0 host/stop, 1 close
    int group = 4;         // General
    std::string name, description;
    std::string status;
    bool busy = false;
    Rect field[2]{}, group_box{}, action{}, close{};
    tracker::Hosting hosting;
    std::string error;
};

inline Window g;

inline std::string *focused_text() {
    return g.focus == 0 ? &g.name : &g.description;
}

inline void layout(int w, int h) {
    g.lay = chrome_layout(w, h, nullptr, GetForegroundWindow() == g.hwnd);
    g.lay.max_box = {0, 0, 0, 0};
    g.lay.grip = {0, 0, 0, 0};
    int lx = g.lay.client.x + 14, fx = lx + 96;
    int fw = g.lay.client.right() - 14 - fx;
    int y = g.lay.client.y + 16;
    for (int i = 0; i < 2; ++i) {
        g.field[i] = {fx, y, fw, 20};
        y += 28;
    }
    g.group_box = {fx, y, 150, 20};
    g.action = {g.lay.client.right() - 14 - 96, g.lay.client.bottom() - 40, 96,
                26};
    g.close = {g.action.x - 12 - 80, g.action.y, 80, 26};
}

inline void paint() {
    Canvas &cv = g.canvas;
    RECT rc;
    GetClientRect(g.hwnd, &rc);
    if (rc.right <= 0 || rc.bottom <= 0) return;
    if (cv.width() != rc.right || cv.height() != rc.bottom)
        cv.resize(rc.right, rc.bottom);
    bool focused = GetForegroundWindow() == g.hwnd;
    layout(rc.right, rc.bottom);
    paint_chrome(cv, g.lay, "Host a Server", focused, 0,
                 g.pressed_box == 5 ? 1 : 0, settings::active_theme());

    DialogColors dc = dialog_colors(settings::active_theme());
    cv.fill(g.lay.client, dc.workspace);

    const char *labels[] = {"Server Name:", "Description:"};
    for (int i = 0; i < 2; ++i) {
        const std::string &text = i == 0 ? g.name : g.description;
        cv.text(g.lay.client.x + 14,
                g.field[i].y + (g.field[i].h - kFontHeight) / 2, labels[i],
                dc.label);
        draw_field(cv, g.field[i], text.c_str(),
                   focused && g.focus == i && !g.hosting.active, g.caret, dc);
    }
    cv.text(g.lay.client.x + 14,
            g.group_box.y + (g.group_box.h - kFontHeight) / 2, "Group:",
            dc.label);
    draw_button(cv, g.group_box, kGroupNames[g.group], false, false, dc);

    if (!g.status.empty()) {
        cv.set_clip({g.lay.client.x, g.action.y - 24, g.lay.client.w, 16});
        cv.text(g.lay.client.x + 14, g.action.y - 22, g.status.c_str(),
                dc.label);
        cv.clear_clip();
    }

    draw_button(cv, g.action,
                g.busy ? "Working..."
                       : (g.hosting.active ? "Stop" : "Host Server"),
                g.pressed_btn == 0, !g.hosting.active, dc);
    draw_button(cv, g.close, "Close", g.pressed_btn == 1, false, dc);
}

// Registration runs off the UI thread: WinHTTP can block for seconds.
inline DWORD WINAPI register_thread(LPVOID) {
    std::string error;
    bool ok = tracker::register_room(g.hosting, error);
    g.error = error;
    if (g.hwnd) PostMessage(g.hwnd, WM_HOST_DONE, ok ? 1 : 0, 0);
    return 0;
}

inline void start_hosting() {
    if (g.busy || g.hosting.active) return;
    if (g.name.empty()) {
        g.status = "Give the server a name first.";
        return;
    }
    g.hosting.name = g.name;
    g.hosting.description = g.description;
    g.hosting.group = kGroupNames[g.group];
    g.hosting.users = 1;
    g.busy = true;
    g.status = "Registering with the tracker...";
    if (HANDLE t = CreateThread(nullptr, 0, register_thread, nullptr, 0,
                                nullptr))
        CloseHandle(t);
}

inline void stop_hosting() {
    tracker::unregister(g.hosting);
    g.status = "Server removed from the tracker.";
}

// Same as stop_hosting, but the HTTP call runs off the UI thread so closing
// the chat window cannot hitch the message pump on a tracker round-trip.
inline DWORD WINAPI unregister_thread(LPVOID p) {
    auto *h = static_cast<tracker::Hosting *>(p);
    tracker::unregister(*h);
    delete h;
    return 0;
}

inline void stop_hosting_async() {
    if (!g.hosting.active) return;
    auto *h = new tracker::Hosting(g.hosting);
    g.hosting.active = false;
    g.hosting.id.clear();
    g.hosting.token.clear();
    g.status = "Server removed from the tracker.";
    if (HANDLE t = CreateThread(nullptr, 0, unregister_thread, h, 0, nullptr))
        CloseHandle(t);
    else {
        tracker::unregister(*h);
        delete h;
    }
}

}  // namespace host_room
