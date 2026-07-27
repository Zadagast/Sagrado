// Relay smoke test against a running `wrangler dev`. Needs Node's WebSocket:
//     node --experimental-websocket test/relay.mjs
const base = process.env.TRACKER_URL ?? "http://127.0.0.1:8787";
const ws_base = base.replace(/^http/, "ws");

const post = async (path, body) => {
    const r = await fetch(base + path, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify(body),
    });
    return { status: r.status, body: await r.json() };
};

const check = (ok, what) => {
    console.log(`${ok ? "ok  " : "FAIL"} ${what}`);
    if (!ok) process.exitCode = 1;
};

// Resolves with the next binary frame, or rejects if none arrives in time.
const next_frame = (ws, ms = 4000) => new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error("timed out")), ms);
    ws.addEventListener("message", (ev) => {
        clearTimeout(timer);
        resolve(new Uint8Array(ev.data));
    }, { once: true });
});

const open = (url) => new Promise((resolve, reject) => {
    const ws = new WebSocket(url);
    ws.binaryType = "arraybuffer";
    ws.addEventListener("open", () => resolve(ws), { once: true });
    ws.addEventListener("error", () => reject(new Error("refused " + url)),
                        { once: true });
});

const text = (bytes) => new TextDecoder().decode(bytes);
const envelope = (peer, kind, payload) => {
    const body = new TextEncoder().encode(payload ?? "");
    const out = new Uint8Array(5 + body.length);
    new DataView(out.buffer).setUint32(0, peer, true);
    out[4] = kind;
    out.set(body, 5);
    return out;
};

const reg = await post("/register", { name: "Relay test", group: "General" });
check(reg.status === 200, "register a server to host");
const { id, token } = reg.body;

let refused = false;
try { await open(`${ws_base}/relay/${id}?role=guest`); } catch { refused = true; }
check(refused, "guests are refused while the server is offline");

const host = await open(`${ws_base}/relay/${id}?role=host&token=${token}`);
check(true, "host connects with its listing token");

let bad_host = false;
try {
    await open(`${ws_base}/relay/${id}?role=host&token=wrong`);
} catch { bad_host = true; }
check(bad_host, "hosting needs the listing token");

const joinP = next_frame(host);
const guest = await open(`${ws_base}/relay/${id}?role=guest`);
const join = await joinP;
const peer = new DataView(join.buffer).getUint32(0, true);
check(join[4] === 2 && peer > 0, "host is told a guest joined");

guest.send(new TextEncoder().encode("CHAT\nhello"));
const from_guest = await next_frame(host);
check(new DataView(from_guest.buffer).getUint32(0, true) === peer &&
      from_guest[4] === 1 && text(from_guest.slice(5)) === "CHAT\nhello",
      "guest frames reach the host tagged with the guest's peer number");

host.send(envelope(peer, 1, "WELCOME\nRelay test"));
check(text(await next_frame(guest)) === "WELCOME\nRelay test",
      "host frames reach the addressed guest");

host.send(envelope(0, 1, "CHAT\nEdward\thi all"));
check(text(await next_frame(guest)) === "CHAT\nEdward\thi all",
      "peer 0 broadcasts to every guest");

host.send(envelope(0, 4, "7"));  // KIND_HEARTBEAT with users=7
await new Promise((r) => setTimeout(r, 200));
let list = await (await fetch(base + "/rooms")).json();
const listed = list.rooms.find((r) => r.id === id);
check(listed && listed.users === 7,
      "host heartbeat on the relay updates the directory");

const leftP = next_frame(host);
guest.close();
const left = await leftP;
check(new DataView(left.buffer).getUint32(0, true) === peer && left[4] === 3,
      "host is told the guest left");

host.close();
await post("/remove", { id, token });
process.exit(process.exitCode ?? 0);