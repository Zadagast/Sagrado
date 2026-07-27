// A Sagrado server session: the chat, the user list, and the connection that
// carries them.
//
// Both sides only ever dial out to the tracker's relay — the host opens
// ?role=host, guests open ?role=guest — so hosting works from behind any
// router without a forwarded port. The relay wraps every frame it hands the
// host in [u32 peer][u8 kind][payload]; guests see the bare payload.
//
// While hosting, the listing heartbeat rides this same socket (kind 4) so the
// client never opens a second WinHTTP request to the tracker alongside the
// WebSocket — under Wine that pool clash corrupts the frame stream.
//
// The payload itself is line-based text, fields inside a line separated by
// tabs (colours are RRGGBB hex, as KDX lets everyone pick their own):
//     guest -> host   HELLO\n<nick>\t<fg>\t<bg>     CHAT\n<text>
//     host  -> guest  WELCOME\n<server>             INFO\n<text>
//                     USERS\n<nick>\t<fg>\t<bg>\n...
//                     CHAT\n<nick>\t<fg>\t<text>
//
// The socket lives on a worker thread; it takes the lock to touch the log or
// the user list and posts WM_ROOM_EVENT so the window repaints.
#pragma once

#include <string>
#include <vector>

#include "tracker.h"
#include "ws.h"

namespace room {

constexpr UINT WM_ROOM_EVENT = WM_APP + 3;
constexpr int kMaxLog = 500;

// KDX's defaults: green on black, and grey for the client's own notices.
constexpr uint32_t kDefaultFg = 0x00ff00, kDefaultBg = 0x000000;
constexpr uint32_t kNoticeFg = 0xaaaaaa;

enum Role { None, Host, Guest };
enum Kind : uint8_t {
    KindData = 1,
    KindJoin = 2,
    KindLeave = 3,
    KindHeartbeat = 4,  // host → relay only; keeps the directory listing alive
};

// How often the host pings the relay so the listing does not lapse (matches
// the tracker's heartbeat_ms = ROOM_TTL / 3).
constexpr int kHeartbeatSec = 30;

struct User {
    std::string nick;
    uint32_t fg = kDefaultFg, bg = kDefaultBg;
};

struct Line {
    std::string text;
    uint32_t fg = kNoticeFg;
};

struct Peer {
    unsigned id = 0;
    User who;
    bool greeted = false;
};

struct Session {
    ws::Client sock;
    CRITICAL_SECTION lock{};
    CRITICAL_SECTION send_lock{};
    bool ready = false;  // critical sections initialised
    HWND notify = nullptr;
    Role role = None;
    bool running = false;
    bool connected = false;
    std::string status, server_name, id, token;
    User me;
    std::vector<Peer> peers;   // host only: the connected guests
    std::vector<User> users;   // the list as shown in the window
    std::vector<Line> log;
    HANDLE thread = nullptr;
};

inline Session g;

inline void init() {
    if (g.ready) return;
    InitializeCriticalSection(&g.lock);
    InitializeCriticalSection(&g.send_lock);
    g.ready = true;
}

struct Guard {
    CRITICAL_SECTION *cs;
    explicit Guard(CRITICAL_SECTION *c) : cs(c) { EnterCriticalSection(cs); }
    ~Guard() { LeaveCriticalSection(cs); }
};

inline void wake() {
    if (g.notify) PostMessage(g.notify, WM_ROOM_EVENT, 0, 0);
}

inline void add_line(const std::string &text, uint32_t fg = kNoticeFg) {
    {
        Guard lk(&g.lock);
        g.log.push_back({text, fg});
        if (g.log.size() > kMaxLog)
            g.log.erase(g.log.begin(), g.log.begin() + 100);
    }
    wake();
}

inline void set_status(const std::string &text) {
    {
        Guard lk(&g.lock);
        g.status = text;
    }
    wake();
}

// ---- little text helpers ----

inline std::vector<std::string> split(const std::string &s, char sep) {
    std::vector<std::string> out;
    size_t start = 0;
    for (;;) {
        size_t end = s.find(sep, start);
        out.push_back(s.substr(start, end == std::string::npos
                                          ? std::string::npos
                                          : end - start));
        if (end == std::string::npos) return out;
        start = end + 1;
    }
}

inline std::string hex_colour(uint32_t c) {
    char buf[8];
    wsprintfA(buf, "%06X", c & 0xffffff);
    return buf;
}

inline uint32_t parse_colour(const std::string &s, uint32_t fallback) {
    if (s.size() != 6) return fallback;
    uint32_t v = 0;
    for (char c : s) {
        int d = c >= '0' && c <= '9'   ? c - '0'
                : c >= 'a' && c <= 'f' ? c - 'a' + 10
                : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                       : -1;
        if (d < 0) return fallback;
        v = v * 16 + uint32_t(d);
    }
    return v;
}

// This client's display name: the Windows user name until Settings exists.
inline std::string local_nick() {
    char buf[64] = {0};
    DWORD n = sizeof(buf);
    if (!GetUserNameA(buf, &n) || !buf[0]) return "New Sagrado User";
    return buf;
}

// WinHTTP upgrades an ordinary http(s) request, so the relay URL keeps the
// tracker's own scheme rather than ws:// or wss://.
inline std::string relay_url(const std::string &id, const char *role,
                             const std::string &token) {
    std::string url = tracker::base_url();
    url += "/relay/" + id + "?role=" + role;
    if (*role == 'h') url += "&token=" + token;
    return url;
}

inline std::string chat_line(const User &u, const std::string &text) {
    return "<" + u.nick + "> " + text;
}

// ---- host ----

inline bool send_to(unsigned peer, const std::string &payload) {
    std::string frame(5 + payload.size(), '\0');
    frame[0] = char(peer & 0xff);
    frame[1] = char((peer >> 8) & 0xff);
    frame[2] = char((peer >> 16) & 0xff);
    frame[3] = char((peer >> 24) & 0xff);
    frame[4] = char(KindData);
    memcpy(&frame[5], payload.data(), payload.size());
    Guard lk(&g.send_lock);
    return g.sock.send(frame);
}

// Refresh the tracker listing over the open relay socket. Payload is the
// ASCII user count so the directory stays accurate without a WinHTTP call.
inline bool send_heartbeat() {
    if (g.role != Host || !g.connected) return false;
    int users = 1;
    {
        Guard lk(&g.lock);
        if (!g.users.empty()) users = int(g.users.size());
    }
    char num[16];
    wsprintfA(num, "%d", users);
    std::string frame(5, '\0');
    frame[4] = char(KindHeartbeat);
    frame += num;
    Guard lk(&g.send_lock);
    return g.sock.send(frame);
}

inline DWORD WINAPI heartbeat_thread(LPVOID) {
    while (g.running && g.connected && g.role == Host) {
        send_heartbeat();
        for (int i = 0; i < kHeartbeatSec && g.running && g.connected; ++i)
            Sleep(1000);
    }
    return 0;
}

inline std::string user_field(const User &u) {
    return u.nick + "\t" + hex_colour(u.fg) + "\t" + hex_colour(u.bg);
}

inline std::string user_list_payload() {
    std::string s = "USERS\n" + user_field(g.me);
    for (const Peer &p : g.peers)
        if (p.greeted) s += "\n" + user_field(p.who);
    return s;
}

inline void refresh_users() {
    {
        Guard lk(&g.lock);
        g.users.clear();
        g.users.push_back(g.me);
        for (const Peer &p : g.peers)
            if (p.greeted) g.users.push_back(p.who);
    }
    wake();
}

inline void host_loop() {
    std::vector<uint8_t> msg;
    while (g.running && g.sock.receive(msg)) {
        if (msg.size() < 5) continue;
        unsigned peer = unsigned(msg[0]) | unsigned(msg[1]) << 8 |
                        unsigned(msg[2]) << 16 | unsigned(msg[3]) << 24;
        uint8_t kind = msg[4];
        std::string payload(msg.begin() + 5, msg.end());
        if (kind == KindJoin) {
            g.peers.push_back({peer, User{}, false});
            continue;
        }
        if (kind == KindLeave) {
            for (size_t i = 0; i < g.peers.size(); ++i)
                if (g.peers[i].id == peer) {
                    Peer gone = g.peers[i];
                    g.peers.erase(g.peers.begin() + i);
                    if (gone.greeted) {
                        add_line(gone.who.nick + " left.");
                        send_to(0, "INFO\n" + gone.who.nick + " left.");
                    }
                    break;
                }
            refresh_users();
            send_to(0, user_list_payload());
            continue;
        }
        size_t nl = payload.find('\n');
        std::string verb = payload.substr(0, nl == std::string::npos
                                                 ? payload.size()
                                                 : nl);
        std::string rest = nl == std::string::npos ? "" : payload.substr(nl + 1);
        if (verb == "HELLO") {
            std::vector<std::string> f = split(rest, '\t');
            User who;
            who.nick = f[0].empty() ? "New Sagrado User" : f[0];
            if (f.size() > 1) who.fg = parse_colour(f[1], kDefaultFg);
            if (f.size() > 2) who.bg = parse_colour(f[2], kDefaultBg);
            for (Peer &p : g.peers)
                if (p.id == peer) {
                    p.who = who;
                    p.greeted = true;
                }
            send_to(peer, "WELCOME\n" + g.server_name);
            add_line(who.nick + " joined.");
            send_to(0, "INFO\n" + who.nick + " joined.");
            refresh_users();
            send_to(0, user_list_payload());
        } else if (verb == "CHAT" && !rest.empty()) {
            for (const Peer &p : g.peers)
                if (p.id == peer && p.greeted) {
                    add_line(chat_line(p.who, rest), p.who.fg);
                    send_to(0, "CHAT\n" + p.who.nick + "\t" +
                                   hex_colour(p.who.fg) + "\t" + rest);
                }
        }
    }
}

// ---- guest ----

inline void guest_loop() {
    {
        Guard lk(&g.send_lock);
        g.sock.send("HELLO\n" + user_field(g.me));
    }
    std::vector<uint8_t> msg;
    while (g.running && g.sock.receive(msg)) {
        std::string payload(msg.begin(), msg.end());
        size_t nl = payload.find('\n');
        std::string verb = payload.substr(0, nl == std::string::npos
                                                 ? payload.size()
                                                 : nl);
        std::string rest = nl == std::string::npos ? "" : payload.substr(nl + 1);
        if (verb == "WELCOME") {
            Guard lk(&g.lock);
            g.server_name = rest;
        } else if (verb == "USERS") {
            std::vector<std::string> rows = split(rest, '\n');
            Guard lk(&g.lock);
            g.users.clear();
            for (const std::string &row : rows) {
                if (row.empty()) continue;
                std::vector<std::string> f = split(row, '\t');
                User u;
                u.nick = f[0];
                if (f.size() > 1) u.fg = parse_colour(f[1], kDefaultFg);
                if (f.size() > 2) u.bg = parse_colour(f[2], kDefaultBg);
                g.users.push_back(u);
            }
        } else if (verb == "CHAT") {
            std::vector<std::string> f = split(rest, '\t');
            if (f.size() >= 3) {
                User u;
                u.nick = f[0];
                u.fg = parse_colour(f[1], kDefaultFg);
                add_line(chat_line(u, f[2]), u.fg);
            }
        } else if (verb == "INFO") {
            add_line(rest);
        }
        wake();
    }
}

inline DWORD WINAPI session_thread(LPVOID) {
    const char *role = g.role == Host ? "host" : "guest";
    set_status(g.role == Host ? "Opening the relay..."
                              : "Connecting to the server...");
    if (!g.sock.open(relay_url(g.id, role, g.token))) {
        g.connected = false;
        g.running = false;
        set_status(g.sock.error());
        add_line("Could not connect: " + g.sock.error() + ".");
        return 0;
    }
    g.connected = true;
    if (g.role == Host) {
        set_status("Hosting " + g.server_name + ".");
        add_line("Hosting \"" + g.server_name + "\". Waiting for guests.");
        refresh_users();
        HANDLE beat =
            CreateThread(nullptr, 0, heartbeat_thread, nullptr, 0, nullptr);
        host_loop();
        g.connected = false;  // stop the heartbeat thread's send loop
        if (beat) {
            WaitForSingleObject(beat, 2000);
            CloseHandle(beat);
        }
    } else {
        set_status("Connected.");
        add_line("Connected to \"" + g.server_name + "\".");
        guest_loop();
    }
    g.connected = false;
    if (g.running) add_line("Disconnected.");
    g.running = false;
    set_status("Disconnected.");
    return 0;
}

inline void leave() {
    if (!g.running && !g.connected) return;
    g.running = false;
    g.sock.close();
    if (g.thread) {
        WaitForSingleObject(g.thread, 2000);
        CloseHandle(g.thread);
        g.thread = nullptr;
    }
    Guard lk(&g.lock);
    g.peers.clear();
    g.users.clear();
    g.role = None;
}

inline void start(Role role, HWND notify, const std::string &id,
                  const std::string &token, const std::string &server_name) {
    init();
    leave();
    g.notify = notify;
    g.role = role;
    g.id = id;
    g.token = token;
    g.server_name = server_name;
    g.me.nick = local_nick();
    g.running = true;
    {
        Guard lk(&g.lock);
        g.log.clear();
        g.users.clear();
    }
    g.thread = CreateThread(nullptr, 0, session_thread, nullptr, 0, nullptr);
}

inline void say(const std::string &text) {
    if (text.empty() || !g.connected) return;
    if (g.role == Host) {
        add_line(chat_line(g.me, text), g.me.fg);
        send_to(0, "CHAT\n" + g.me.nick + "\t" + hex_colour(g.me.fg) + "\t" +
                       text);
    } else {
        Guard lk(&g.send_lock);
        g.sock.send("CHAT\n" + text);
    }
}

inline std::vector<Line> log_copy() {
    Guard lk(&g.lock);
    return g.log;
}

inline std::vector<User> user_copy() {
    Guard lk(&g.lock);
    return g.users;
}

inline std::string status_copy() {
    Guard lk(&g.lock);
    return g.status;
}

inline std::string name_copy() {
    Guard lk(&g.lock);
    return g.server_name;
}

}  // namespace room
