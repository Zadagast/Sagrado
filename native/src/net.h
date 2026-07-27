// Talking to the Sagrado tracker: a small WinHTTP wrapper plus just enough
// JSON to read its replies. WinHTTP keeps this dependency-free and works
// under Wine; requests run on a worker thread and post a message back to the
// window so the framebuffer UI never blocks.
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>

#include <string>
#include <vector>

namespace net {

// One blocking request. `body` empty means GET, otherwise POST JSON.
inline bool request(const std::string &url, const std::string &body,
                    std::string &out) {
    out.clear();
    std::wstring wurl(url.begin(), url.end());
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256] = {0}, path[1024] = {0};
    uc.lpszHostName = host;
    uc.dwHostNameLength = 255;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 1023;
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc)) return false;

    HINTERNET session = WinHttpOpen(L"SagradoKDX/1.0",
                                    WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return false;
    bool ok = false;
    HINTERNET conn = WinHttpConnect(session, host, uc.nPort, 0);
    if (conn) {
        DWORD flags = uc.nScheme == INTERNET_SCHEME_HTTPS
                          ? WINHTTP_FLAG_SECURE
                          : 0;
        HINTERNET req = WinHttpOpenRequest(
            conn, body.empty() ? L"GET" : L"POST", path, nullptr,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (req) {
            const wchar_t *hdr = L"Content-Type: application/json\r\n";
            if (WinHttpSendRequest(req, body.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS
                                                     : hdr,
                                   body.empty() ? 0 : DWORD(-1),
                                   body.empty() ? WINHTTP_NO_REQUEST_DATA
                                                : (LPVOID)body.data(),
                                   DWORD(body.size()), DWORD(body.size()), 0) &&
                WinHttpReceiveResponse(req, nullptr)) {
                DWORD avail = 0;
                char buf[4096];
                while (WinHttpQueryDataAvailable(req, &avail) && avail) {
                    DWORD got = 0;
                    DWORD want = avail < sizeof(buf) ? avail : sizeof(buf);
                    if (!WinHttpReadData(req, buf, want, &got) || !got) break;
                    out.append(buf, got);
                }
                ok = true;
            }
            WinHttpCloseHandle(req);
        }
        WinHttpCloseHandle(conn);
    }
    WinHttpCloseHandle(session);
    return ok;
}

}  // namespace net

// A hand-rolled reader for the tracker's replies: enough JSON to walk objects
// and arrays and pull out strings and numbers, with no allocations beyond the
// values asked for.
namespace json {

inline const char *skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
    return p;
}

// Step over one value (object, array, string, number, literal).
inline const char *skip_value(const char *p) {
    p = skip_ws(p);
    if (*p == '"') {
        for (++p; *p && *p != '"'; ++p)
            if (*p == '\\' && p[1]) ++p;
        return *p ? p + 1 : p;
    }
    if (*p == '{' || *p == '[') {
        char close = *p == '{' ? '}' : ']';
        ++p;
        while (*p) {
            p = skip_ws(p);
            if (*p == close) return p + 1;
            p = *p == ',' ? p + 1 : skip_value(p);
        }
        return p;
    }
    while (*p && *p != ',' && *p != '}' && *p != ']') ++p;
    return p;
}

// The value of `key` inside the object starting at `obj`, or nullptr. Only
// the object's own members are searched; nested objects are stepped over.
inline const char *member(const char *obj, const char *key) {
    const char *p = skip_ws(obj);
    if (*p != '{') return nullptr;
    ++p;
    size_t klen = strlen(key);
    while (*p) {
        p = skip_ws(p);
        if (*p == '}') return nullptr;
        if (*p != '"') return nullptr;
        const char *name = p + 1;
        const char *end = skip_value(p) - 1;
        p = skip_ws(skip_value(p));
        if (*p != ':') return nullptr;
        ++p;
        bool hit = size_t(end - name) == klen &&
                   strncmp(name, key, klen) == 0;
        p = skip_ws(p);
        if (hit) return p;
        p = skip_ws(skip_value(p));
        if (*p == ',') ++p;
    }
    return nullptr;
}

inline std::string as_string(const char *v) {
    std::string s;
    if (!v || *v != '"') return s;
    for (++v; *v && *v != '"'; ++v) {
        if (*v == '\\' && v[1]) {
            ++v;
            switch (*v) {
                case 'n': s += '\n'; break;
                case 't': s += '\t'; break;
                case 'u': {  // keep it ASCII; the list font has no glyphs
                    if (strlen(v) >= 5) v += 4;
                    s += '?';
                    break;
                }
                default: s += *v;
            }
        } else {
            s += *v;
        }
    }
    return s;
}

inline long long as_number(const char *v) {
    return v ? _atoi64(v) : 0;
}

inline std::string string_member(const char *obj, const char *key) {
    return as_string(member(obj, key));
}

inline long long number_member(const char *obj, const char *key) {
    return as_number(member(obj, key));
}

// Pointers to each element of the array at `arr`.
inline std::vector<const char *> elements(const char *arr) {
    std::vector<const char *> out;
    if (!arr) return out;
    const char *p = skip_ws(arr);
    if (*p != '[') return out;
    ++p;
    while (*p) {
        p = skip_ws(p);
        if (*p == ']' || !*p) break;
        out.push_back(p);
        p = skip_ws(skip_value(p));
        if (*p == ',') ++p;
    }
    return out;
}

}  // namespace json
