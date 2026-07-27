// Console harness for ws.h: connects to a relay URL, optionally sends one
// message, then prints every frame that comes back. Handy for separating a
// relay problem from a client one. Build with `make build/test_ws.exe`.
//
// With a third argument it also hammers the tracker's HTTP API from another
// thread while the socket is open, which is how the WinHTTP connection-pool
// clash between the two was found.
#include <cstdio>
#include <string>

#include "net.h"
#include "ws.h"

namespace {

std::string g_poll_url;

DWORD WINAPI poll_thread(LPVOID) {
    for (;;) {
        std::string body;
        bool ok = net::request(g_poll_url, "", body);
        printf("[http] %s %d bytes\n", ok ? "ok" : "failed", int(body.size()));
        fflush(stdout);
        Sleep(700);
    }
}

}  // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("usage: test_ws <http(s) relay url> [message] [poll url]\n");
        return 2;
    }
    ws::Client c;
    if (!c.open(argv[1])) {
        printf("open failed: %s\n", c.error().c_str());
        return 1;
    }
    printf("connected\n");
    if (argc > 2) printf("send -> %d\n", int(c.send(std::string(argv[2]))));
    fflush(stdout);
    if (argc > 3) {
        g_poll_url = argv[3];
        CreateThread(nullptr, 0, poll_thread, nullptr, 0, nullptr);
    }
    std::vector<uint8_t> in;
    while (c.receive(in)) {
        printf("recv %d bytes:", int(in.size()));
        for (uint8_t b : in) printf(" %02x", b);
        printf("  %.*s\n", int(in.size()), (const char *)in.data());
        // Answer the way the host does: a burst of frames per message.
        if (in.size() >= 5 && in[4] == 1)
            for (int i = 0; i < 3; ++i) {
                std::string msg(5, '\0');
                msg[4] = 1;
                msg += "reply" + std::to_string(i);
                printf("  send -> %d\n", int(c.send(msg)));
            }
        fflush(stdout);
    }
    printf("done: %s\n", c.error().c_str());
    return 0;
}
