// The tracker client: fetches the room directory from the Sagrado tracker
// (a Cloudflare Worker) on a background thread and hands the parsed list to
// the Connect... window. Hosting a room registers here too and keeps the
// entry alive with heartbeats.
#pragma once

#include <time.h>

#include <string>
#include <vector>

#include "net.h"

namespace tracker {

// Where the directory lives. Overridden by a tracker.txt next to the exe so
// a room can be pointed at a private tracker (or a local wrangler dev).
inline const char *kDefaultUrl = "https://sagrado-tracker.zadagast.workers.dev";

// KDX's fixed group list, shown even when the tracker is unreachable.
inline const char *const kGroupNames[] = {"Business", "Chat",      "Education",
                                          "Games",    "General",   "Macintosh",
                                          "Trackers", "Windows"};
constexpr int kGroupCount = 8;

struct Room {
    std::string name, group, description, addr, pubkey, date;
    int users = 0;
};

struct Group {
    std::string name;
    int count = 0;
};

// Fetch state shown in the window while the request is in flight.
enum Status { Idle, Fetching, Ready, Failed };

struct Directory {
    std::vector<Group> groups;
    std::vector<Room> rooms;
    Status status = Idle;
    std::string error;
};

inline std::string base_url() {
    char exe[MAX_PATH];
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    std::string path(exe);
    size_t slash = path.find_last_of('\\');
    if (slash != std::string::npos) path.resize(slash + 1);
    path += "tracker.txt";
    std::string url;
    if (HANDLE f = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, 0, nullptr);
        f != INVALID_HANDLE_VALUE) {
        char buf[512];
        DWORD got = 0;
        if (ReadFile(f, buf, sizeof(buf) - 1, &got, nullptr) && got) {
            buf[got] = 0;
            url = buf;
        }
        CloseHandle(f);
    }
    if (url.empty()) url = kDefaultUrl;
    while (!url.empty() && (url.back() == '\n' || url.back() == '\r' ||
                            url.back() == ' ' || url.back() == '/'))
        url.pop_back();
    return url;
}

// "26/07/25 10:17 AM", the format the real tracker shows.
inline std::string format_date(long long epoch_ms) {
    time_t t = time_t(epoch_ms / 1000);
    struct tm lt;
    if (!localtime_s(&lt, &t)) {
        char buf[32];
        int hour = lt.tm_hour % 12;
        if (hour == 0) hour = 12;
        wsprintfA(buf, "%02d/%02d/%02d %02d:%02d %s", lt.tm_mday,
                  lt.tm_mon + 1, lt.tm_year % 100, hour, lt.tm_min,
                  lt.tm_hour < 12 ? "AM" : "PM");
        return buf;
    }
    return "";
}

inline bool parse(const std::string &body, Directory &dir) {
    const char *root = body.c_str();
    const char *groups = json::member(root, "groups");
    const char *rooms = json::member(root, "rooms");
    if (!groups || !rooms) return false;
    dir.groups.clear();
    dir.rooms.clear();
    for (const char *g : json::elements(groups))
        dir.groups.push_back({json::string_member(g, "name"),
                              int(json::number_member(g, "count"))});
    for (const char *r : json::elements(rooms)) {
        Room room;
        room.name = json::string_member(r, "name");
        room.group = json::string_member(r, "group");
        room.description = json::string_member(r, "description");
        room.addr = json::string_member(r, "addr");
        room.pubkey = json::string_member(r, "pubkey");
        room.users = int(json::number_member(r, "users"));
        room.date = format_date(json::number_member(r, "since"));
        dir.rooms.push_back(room);
    }
    return true;
}

// A room this client is hosting, kept alive by heartbeats.
struct Hosting {
    bool active = false;
    std::string id, token, name, group, description;
    int port = 4880;
    int users = 1;
};

inline bool register_room(Hosting &h, std::string &error) {
    char body[1024];
    wsprintfA(body,
              "{\"name\":\"%s\",\"group\":\"%s\",\"description\":\"%s\","
              "\"users\":%d,\"addr\":\"%s\"}",
              h.name.c_str(), h.group.c_str(), h.description.c_str(), h.users,
              (":" + std::to_string(h.port)).c_str());
    std::string reply;
    if (!net::request(base_url() + "/register", body, reply)) {
        error = "could not reach the tracker";
        return false;
    }
    h.id = json::string_member(reply.c_str(), "id");
    h.token = json::string_member(reply.c_str(), "token");
    if (h.id.empty()) {
        error = json::string_member(reply.c_str(), "error");
        if (error.empty()) error = "tracker refused the room";
        return false;
    }
    h.active = true;
    return true;
}

inline void heartbeat(const Hosting &h) {
    if (!h.active) return;
    char body[512];
    wsprintfA(body, "{\"id\":\"%s\",\"token\":\"%s\",\"users\":%d}",
              h.id.c_str(), h.token.c_str(), h.users);
    std::string reply;
    net::request(base_url() + "/heartbeat", body, reply);
}

inline void unregister(Hosting &h) {
    if (!h.active) return;
    char body[512];
    wsprintfA(body, "{\"id\":\"%s\",\"token\":\"%s\"}", h.id.c_str(),
              h.token.c_str());
    std::string reply;
    net::request(base_url() + "/remove", body, reply);
    h.active = false;
}

}  // namespace tracker
