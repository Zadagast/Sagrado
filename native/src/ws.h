// A WebSocket client on WinHTTP: the only socket Sagrado opens is an outgoing
// one to the tracker's relay, which is what lets a server be hosted from
// behind any router without forwarding a port. Blocking by design — callers
// run it on a worker thread and post results back to the window.
//
// KNOWN BUG (Wine only, reproducible with build/test_ws.exe): while this
// socket is open, an ordinary WinHTTP request to the same tracker corrupts the
// frame stream — the relay rejects the next frame ("RSV bits set" / "the
// compression bit was set") and drops the connection. It looks like Wine's
// global keep-alive pool handing the WebSocket's connection to the HTTP
// request; neither WINHTTP_DISABLE_KEEP_ALIVE on both handles nor holding the
// upgrade request handle open for the socket's lifetime (both done below)
// fixes it. Native Windows is unaffected. Likely fixes: stop making HTTP
// calls while hosting (move the tracker heartbeat onto the relay socket), or
// replace WinHTTP here with a Winsock WebSocket plus Schannel for wss://.
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>

#include <string>
#include <vector>

namespace ws {

constexpr size_t kMaxMessage = 256 * 1024;

class Client {
  public:
    ~Client() { close(); }

    // url is the http(s) URL WinHTTP upgrades, query string included.
    bool open(const std::string &url) {
        close();
        std::wstring wurl(url.begin(), url.end());
        URL_COMPONENTS uc{};
        uc.dwStructSize = sizeof(uc);
        wchar_t host[256] = {0}, path[1024] = {0}, query[1024] = {0};
        uc.lpszHostName = host;
        uc.dwHostNameLength = 255;
        uc.lpszUrlPath = path;
        uc.dwUrlPathLength = 1023;
        uc.lpszExtraInfo = query;
        uc.dwExtraInfoLength = 1023;
        if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc)) return fail("bad url");
        bool secure = uc.nScheme == INTERNET_SCHEME_HTTPS;
        std::wstring target = path;
        target += query;

        session_ = WinHttpOpen(L"SagradoKDX/1.0",
                               WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                               WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS,
                               0);
        if (!session_) return fail("no WinHTTP session");
        conn_ = WinHttpConnect(session_, host, uc.nPort, 0);
        if (!conn_) return fail("could not reach the relay");

        HINTERNET req = WinHttpOpenRequest(
            conn_, L"GET", target.c_str(), nullptr, WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0);
        if (!req) return fail("could not open the relay request");
        // Keep this connection out of WinHTTP's keep-alive pool: under Wine a
        // later tracker request can otherwise be handed the socket the
        // WebSocket is still using, and the two write over each other.
        DWORD no_keepalive = WINHTTP_DISABLE_KEEP_ALIVE;
        WinHttpSetOption(req, WINHTTP_OPTION_DISABLE_FEATURE, &no_keepalive,
                         sizeof(no_keepalive));

        bool ok = WinHttpSetOption(req, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET,
                                   nullptr, 0) &&
                  WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                     WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                  WinHttpReceiveResponse(req, nullptr);
        if (ok) {
            DWORD status = 0, len = sizeof(status);
            WinHttpQueryHeaders(req,
                                WINHTTP_QUERY_STATUS_CODE |
                                    WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX, &status, &len,
                                WINHTTP_NO_HEADER_INDEX);
            if (status != 101) {
                ok = false;
                error_ = status == 503   ? "that server is offline"
                         : status == 403 ? "the relay rejected this client"
                                         : "the relay refused the connection";
            }
        } else {
            error_ = "could not reach the relay";
        }
        if (ok) {
            socket_ = WinHttpWebSocketCompleteUpgrade(req, 0);
            if (!socket_) {
                ok = false;
                error_ = "the relay handshake failed";
            }
        }
        // The request handle stays open for the socket's lifetime; closing it
        // here is what lets Wine recycle the underlying connection.
        if (ok)
            req_ = req;
        else
            WinHttpCloseHandle(req);
        if (!ok) close();
        return ok;
    }

    bool connected() const { return socket_ != nullptr; }
    const std::string &error() const { return error_; }

    bool send(const void *data, size_t len) {
        if (!socket_) return false;
        return WinHttpWebSocketSend(socket_,
                                    WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE,
                                    const_cast<void *>(data),
                                    DWORD(len)) == NO_ERROR;
    }

    bool send(const std::string &s) { return send(s.data(), s.size()); }

    // Unblock a receive() sitting on another thread. Safe to call from the UI
    // thread: it only starts the close handshake and does not tear handles
    // down. close() must run after receive has returned (see room::leave).
    void interrupt() {
        if (!socket_) return;
        WinHttpWebSocketShutdown(socket_,
                                 WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS,
                                 nullptr, 0);
    }

    // Blocks until a whole message arrives (fragments are reassembled), the
    // peer closes, or the socket breaks.
    bool receive(std::vector<uint8_t> &out) {
        out.clear();
        if (!socket_) return false;
        for (;;) {
            uint8_t chunk[8192];
            DWORD got = 0;
            WINHTTP_WEB_SOCKET_BUFFER_TYPE type{};
            DWORD rc = WinHttpWebSocketReceive(socket_, chunk, sizeof(chunk),
                                               &got, &type);
            if (rc != NO_ERROR) {
                error_ = "the relay connection dropped";
                return false;
            }
            if (type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) {
                error_ = "the relay closed the connection";
                return false;
            }
            if (out.size() + got > kMaxMessage) {
                error_ = "oversized frame";
                return false;
            }
            out.insert(out.end(), chunk, chunk + got);
            if (type == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE ||
                type == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE)
                return true;
        }
    }

    void close() {
        if (socket_) {
            // Handles only — the close handshake was interrupt() or the peer.
            // Calling WinHttpWebSocketClose here while another thread is in
            // Receive deadlocks under Wine (and can hang native WinHTTP too).
            WinHttpCloseHandle(socket_);
            socket_ = nullptr;
        }
        if (req_) {
            WinHttpCloseHandle(req_);
            req_ = nullptr;
        }
        if (conn_) {
            WinHttpCloseHandle(conn_);
            conn_ = nullptr;
        }
        if (session_) {
            WinHttpCloseHandle(session_);
            session_ = nullptr;
        }
    }

  private:
    bool fail(const char *why) {
        error_ = why;
        close();
        return false;
    }

    HINTERNET session_ = nullptr, conn_ = nullptr, req_ = nullptr,
              socket_ = nullptr;
    std::string error_;
};

}  // namespace ws
