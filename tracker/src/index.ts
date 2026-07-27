// Sagrado KDX tracker: the directory the Connect... window reads.
//
// Hosts POST /register with their room details, then POST /heartbeat every
// half minute or so; a room disappears once its heartbeat lapses. Everything
// lives in one Durable Object so the list is globally consistent, and the
// tracker only ever sees discovery data — room traffic is peer to peer.

export interface Env {
    REGISTRY: DurableObjectNamespace;
}

// KDX's fixed group list; unknown groups fall back to General.
export const GROUPS = [
    "Business", "Chat", "Education", "Games",
    "General", "Macintosh", "Trackers", "Windows",
];

const ROOM_TTL_MS = 90_000;   // heartbeat lapse before a room is dropped
const MAX_ROOMS = 2000;
const MAX_FIELD = 200;

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
        // A host that doesn't know its public address sends ":port" and the
        // tracker fills in the address the registration arrived from.
        let addr = clean(b.addr, 100);
        if (addr.startsWith(":")) {
            const ip = req.headers.get("cf-connecting-ip");
            addr = ip ? ip + addr : "";
        }
        if (!name || !addr) return json({ error: "name and addr required" }, 400);
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
                "POST /heartbeat, POST /remove.\n",
                { headers: { "content-type": "text/plain; charset=utf-8" } });

        const id = env.REGISTRY.idFromName("global");
        return env.REGISTRY.get(id).fetch(req);
    },
};
