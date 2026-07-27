// Sagrado KDX tracker: the directory the Connect... window reads, plus the
// relay that carries server traffic.
//
// Hosts POST /register with their server's details, then POST /heartbeat
// every half minute or so; a server disappears once its heartbeat lapses.
// The directory lives in one Durable Object so the list is globally
// consistent.
//
// Nobody listens for incoming connections: a host opens an outgoing WebSocket
// to /relay/<id>?role=host and guests open one to /relay/<id>?role=guest, and
// a per-server Durable Object shuffles frames between them. That works behind
// any router or CGNAT without port forwarding. The relay only ever sees
// opaque frames.

export interface Env {
    REGISTRY: DurableObjectNamespace;
    RELAY: DurableObjectNamespace;
}

// KDX's fixed group list; unknown groups fall back to General.
export const GROUPS = [
    "Business", "Chat", "Education", "Games",
    "General", "Macintosh", "Trackers", "Windows",
];

const ROOM_TTL_MS = 90_000;   // heartbeat lapse before a room is dropped
const MAX_ROOMS = 2000;
const MAX_FIELD = 200;
const MAX_GUESTS = 64;
const MAX_FRAME = 256 * 1024;

// Relay envelope: every frame between the host and the relay is
// [u32 peer][u8 kind][payload]. Guests speak the bare payload; the relay tags
// it with the guest's peer number on the way to the host.
const KIND_DATA = 1;
const KIND_JOIN = 2;
const KIND_LEAVE = 3;

function envelope(peer: number, kind: number, payload: ArrayBuffer | null) {
    const body = payload ? new Uint8Array(payload) : new Uint8Array(0);
    const out = new Uint8Array(5 + body.length);
    new DataView(out.buffer).setUint32(0, peer, true);
    out[4] = kind;
    out.set(body, 5);
    return out;
}

interface Room {
    id: string;
    name: string;
    group: string;
    description: string;
    users: number;
    addr: string;     // host address peers dial, "host:port"
    pubkey: string;   // host identity, verified on connect
    since: number;    // first registration, shown as "Date Online"
    seen: number;
}

function clean(v: unknown, max = MAX_FIELD): string {
    if (typeof v !== "string") return "";
    // Control characters would corrupt the fixed-width list rendering.
    return v.replace(/[\x00-\x1f\x7f]/g, "").slice(0, max).trim();
}

function json(body: unknown, status = 200): Response {
    return new Response(JSON.stringify(body), {
        status,
        headers: {
            "content-type": "application/json; charset=utf-8",
            "access-control-allow-origin": "*",
        },
    });
}

export class Registry {
    private state: DurableObjectState;
    private rooms = new Map<string, Room>();
    private tokens = new Map<string, string>();
    private loaded = false;

    constructor(state: DurableObjectState) {
        this.state = state;
    }

    private async load() {
        if (this.loaded) return;
        const rooms = await this.state.storage.get<Record<string, Room>>("rooms");
        const tokens =
            await this.state.storage.get<Record<string, string>>("tokens");
        if (rooms) for (const [k, v] of Object.entries(rooms)) this.rooms.set(k, v);
        if (tokens) for (const [k, v] of Object.entries(tokens)) this.tokens.set(k, v);
        this.loaded = true;
    }

    private async save() {
        await this.state.storage.put({
            rooms: Object.fromEntries(this.rooms),
            tokens: Object.fromEntries(this.tokens),
        });
    }

    private expire() {
        const cutoff = Date.now() - ROOM_TTL_MS;
        for (const [id, room] of this.rooms)
            if (room.seen < cutoff) {
                this.rooms.delete(id);
                this.tokens.delete(id);
            }
    }

    async fetch(req: Request): Promise<Response> {
        await this.load();
        this.expire();
        const url = new URL(req.url);
        switch (url.pathname) {
            case "/register":
                return this.register(req);
            case "/heartbeat":
                return this.heartbeat(req);
            case "/remove":
                return this.remove(req);
            case "/rooms":
                return this.list();
            case "/verify":
                return this.verify(req);
            default:
                return json({ error: "not found" }, 404);
        }
    }

    private async body(req: Request): Promise<Record<string, unknown>> {
        try {
            const v = await req.json();
            return typeof v === "object" && v ? (v as Record<string, unknown>) : {};
        } catch {
            return {};
        }
    }

    private async register(req: Request): Promise<Response> {
        const b = await this.body(req);
        const name = clean(b.name, 64);
        // Guests reach a server through the relay by id, so an address is
        // optional: it only matters for a future direct peer connection.
        let addr = clean(b.addr, 100);
        if (addr.startsWith(":")) {
            const ip = req.headers.get("cf-connecting-ip");
            addr = ip ? ip + addr : "";
        }
        if (!name) return json({ error: "name required" }, 400);
        if (this.rooms.size >= MAX_ROOMS) return json({ error: "tracker full" }, 503);

        const group = GROUPS.includes(clean(b.group)) ? clean(b.group) : "General";
        const id = crypto.randomUUID();
        const token = crypto.randomUUID();
        const now = Date.now();
        this.rooms.set(id, {
            id,
            name,
            group,
            description: clean(b.description),
            users: Math.max(0, Math.min(9999, Number(b.users) || 0)),
            addr,
            pubkey: clean(b.pubkey, 128),
            since: now,
            seen: now,
        });
        this.tokens.set(id, token);
        await this.save();
        return json({ id, token, heartbeat_ms: ROOM_TTL_MS / 3 });
    }

    private async heartbeat(req: Request): Promise<Response> {
        const b = await this.body(req);
        const id = clean(b.id, 64);
        const room = this.rooms.get(id);
        if (!room || this.tokens.get(id) !== clean(b.token, 64))
            return json({ error: "unknown room" }, 404);
        room.seen = Date.now();
        if (b.users !== undefined)
            room.users = Math.max(0, Math.min(9999, Number(b.users) || 0));
        if (b.description !== undefined) room.description = clean(b.description);
        await this.save();
        return json({ ok: true });
    }

    private async remove(req: Request): Promise<Response> {
        const b = await this.body(req);
        const id = clean(b.id, 64);
        if (this.tokens.get(id) !== clean(b.token, 64))
            return json({ error: "unknown room" }, 404);
        this.rooms.delete(id);
        this.tokens.delete(id);
        await this.save();
        return json({ ok: true });
    }

    // Used by the relay route: does this server exist, and (for a host) does
    // the caller hold its listing token?
    private async verify(req: Request): Promise<Response> {
        const b = await this.body(req);
        const id = clean(b.id, 64);
        const room = this.rooms.get(id);
        if (!room) return json({ error: "unknown room" }, 404);
        if (b.token !== undefined && this.tokens.get(id) !== clean(b.token, 64))
            return json({ error: "bad token" }, 403);
        return json({ ok: true, name: room.name, pubkey: room.pubkey });
    }

    private async list(): Promise<Response> {
        await this.save();
        const rooms = [...this.rooms.values()].sort((a, b) =>
            a.name.localeCompare(b.name));
        const groups = GROUPS.map((name) => ({
            name,
            count: rooms.filter((r) => r.group === name).length,
        }));
        return json({
            groups,
            rooms: rooms.map(({ id, name, group, description, users, addr,
                                pubkey, since }) => ({
                id, name, group, description, users, addr, pubkey, since,
            })),
        });
    }
}

// One instance per hosted server: the host's socket on one side, its guests'
// on the other. Frames are forwarded verbatim; the relay never interprets a
// payload.
export class RoomRelay {
    private host: WebSocket | null = null;
    private guests = new Map<number, WebSocket>();
    private next = 1;

    async fetch(req: Request): Promise<Response> {
        if (req.headers.get("upgrade") !== "websocket")
            return json({ error: "expected a websocket upgrade" }, 426);
        const role = new URL(req.url).searchParams.get("role");
        if (role !== "host" && role !== "guest")
            return json({ error: "role must be host or guest" }, 400);
        if (role === "guest" && !this.host)
            return json({ error: "server offline" }, 503);
        if (role === "guest" && this.guests.size >= MAX_GUESTS)
            return json({ error: "server full" }, 503);

        const pair = new WebSocketPair();
        const client = pair[0], server = pair[1];
        server.accept();
        if (role === "host") this.attachHost(server);
        else this.attachGuest(server);
        return new Response(null, { status: 101, webSocket: client });
    }

    // A host reconnecting after a network drop replaces the stale socket; the
    // guests it left behind are dropped so they reconnect cleanly.
    private attachHost(ws: WebSocket) {
        const old = this.host;
        this.host = ws;
        if (old) try { old.close(1012, "host reconnected"); } catch { }
        for (const [, g] of this.guests) try { g.close(1012, "host reconnected"); } catch { }
        this.guests.clear();

        ws.addEventListener("message", (ev: MessageEvent) => {
            if (typeof ev.data === "string" || ev.data.byteLength < 5) return;
            if (ev.data.byteLength > MAX_FRAME) return;
            const view = new DataView(ev.data);
            const peer = view.getUint32(0, true);
            const kind = view.getUint8(4);
            if (kind !== KIND_DATA) return;
            const payload = ev.data.slice(5);
            if (peer === 0) {
                for (const [, g] of this.guests) this.send(g, payload);
            } else {
                const g = this.guests.get(peer);
                if (g) this.send(g, payload);
            }
        });
        const drop = () => {
            if (this.host !== ws) return;
            this.host = null;
            for (const [, g] of this.guests)
                try { g.close(1001, "server went offline"); } catch { }
            this.guests.clear();
        };
        ws.addEventListener("close", drop);
        ws.addEventListener("error", drop);
    }

    private attachGuest(ws: WebSocket) {
        const peer = this.next++;
        this.guests.set(peer, ws);
        this.toHost(envelope(peer, KIND_JOIN, null));

        ws.addEventListener("message", (ev: MessageEvent) => {
            if (typeof ev.data === "string") return;
            if (ev.data.byteLength > MAX_FRAME) return;
            this.toHost(envelope(peer, KIND_DATA, ev.data));
        });
        const drop = () => {
            if (!this.guests.delete(peer)) return;
            this.toHost(envelope(peer, KIND_LEAVE, null));
        };
        ws.addEventListener("close", drop);
        ws.addEventListener("error", drop);
    }

    private toHost(frame: Uint8Array) {
        if (this.host) this.send(this.host, frame);
    }

    private send(ws: WebSocket, data: ArrayBuffer | Uint8Array) {
        try { ws.send(data); } catch { }
    }
}

// The relay route: /relay/<server id>?role=host&token=... (hosts) or
// ?role=guest. Both are checked against the directory before the upgrade.
async function relay(req: Request, env: Env, id: string): Promise<Response> {
    const role = new URL(req.url).searchParams.get("role");
    const token = new URL(req.url).searchParams.get("token");
    const body: Record<string, string> = { id };
    if (role === "host") body.token = token ?? "";
    const registry = env.REGISTRY.get(env.REGISTRY.idFromName("global"));
    const check = await registry.fetch("https://tracker/verify", {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify(body),
    });
    if (!check.ok) return new Response(await check.text(), {
        status: check.status,
        headers: { "content-type": "application/json; charset=utf-8" },
    });
    return env.RELAY.get(env.RELAY.idFromName(id)).fetch(req);
}

export default {
    async fetch(req: Request, env: Env): Promise<Response> {
        const url = new URL(req.url);
        if (req.method === "OPTIONS")
            return new Response(null, {
                headers: {
                    "access-control-allow-origin": "*",
                    "access-control-allow-headers": "content-type",
                    "access-control-allow-methods": "GET,POST,OPTIONS",
                },
            });
        if (url.pathname === "/" || url.pathname === "/index.html")
            return new Response(
                "Sagrado KDX tracker. GET /rooms, POST /register, " +
                "POST /heartbeat, POST /remove, " +
                "WebSocket /relay/<id>?role=host|guest.\n",
                { headers: { "content-type": "text/plain; charset=utf-8" } });

        const room = url.pathname.match(/^\/relay\/([^/]+)$/);
        if (room) return relay(req, env, decodeURIComponent(room[1]));

        const id = env.REGISTRY.idFromName("global");
        return env.REGISTRY.get(id).fetch(req);
    },
};
