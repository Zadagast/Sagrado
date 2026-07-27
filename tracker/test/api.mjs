// Smoke test against a running `wrangler dev` (default http://127.0.0.1:8787).
const base = process.env.TRACKER_URL ?? "http://127.0.0.1:8787";

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

const reg = await post("/register", {
    name: "Loophole's Lair",
    group: "General",
    description: "Test room",
    users: 3,
    addr: "203.0.113.7:4880",
    pubkey: "abc123",
});
check(reg.status === 200 && reg.body.id && reg.body.token, "register");

const bad = await post("/register", { name: "no addr" });
check(bad.status === 400, "register rejects a room with no address");

let list = await (await fetch(base + "/rooms")).json();
check(list.rooms.length === 1 && list.rooms[0].name === "Loophole's Lair",
      "room appears in /rooms");
check(list.groups.find((g) => g.name === "General").count === 1,
      "group counts");

const beat = await post("/heartbeat", {
    id: reg.body.id, token: reg.body.token, users: 9,
});
check(beat.status === 200, "heartbeat");
list = await (await fetch(base + "/rooms")).json();
check(list.rooms[0].users === 9, "heartbeat updates the user count");

check((await post("/heartbeat", { id: reg.body.id, token: "wrong" })).status
      === 404, "heartbeat needs the room's token");

check((await post("/remove", { id: reg.body.id, token: reg.body.token }))
      .status === 200, "remove");
list = await (await fetch(base + "/rooms")).json();
check(list.rooms.length === 0, "removed room is gone");
