# Sagrado tracker

The room directory behind KDX's `Connect...` window: a Cloudflare Worker with
a single Durable Object holding the live room list. It is the only always-on
piece of Sagrado — rooms themselves are hosted by whoever starts them, and
room traffic never passes through here.

## API

| Route        | Method | Body                                                         | Reply |
| ------------ | ------ | ------------------------------------------------------------ | ----- |
| `/rooms`     | GET    | —                                                              | `{groups:[{name,count}], rooms:[…]}` |
| `/register`  | POST   | `{name, group, description, users, addr, pubkey}`              | `{id, token, heartbeat_ms}` |
| `/heartbeat` | POST   | `{id, token, users?, description?}`                            | `{ok:true}` |
| `/remove`    | POST   | `{id, token}`                                                  | `{ok:true}` |
| `/relay/<id>`| WS     | host: `?role=host&token=…` · guest: `?role=guest`              | opaque frames; host may send kind `4` heartbeats |

- `addr` is the address guests dial. A host that doesn't know its public
  address sends `":4880"` and the tracker fills in the address the
  registration came from.
- `token` is the room's private key to its listing: only its holder can
  heartbeat, update or remove it.
- A room disappears 90s after its last heartbeat, so crashed hosts drop off
  the list by themselves. While the host's relay socket is open, heartbeats
  are kind-`4` frames on that socket (ASCII user count in the payload) so
  the client never needs a concurrent HTTP call to the tracker — under Wine
  that clashes with WinHTTP's WebSocket. `POST /heartbeat` remains for tools
  and the brief window before the relay is up.
- `pubkey` is the host's identity; guests verify it during the room handshake,
  so a listing can't be impersonated by whoever holds the address.

## Develop

```sh
cd tracker
npm install
npx wrangler dev      # http://127.0.0.1:8787
npm test              # smoke test against a running dev server
```

Point the client at a tracker by dropping a `tracker.txt` next to
`SagradoKDX.exe` containing the base URL (e.g. `http://127.0.0.1:8787`);
without one it uses the deployed tracker.

## Deploy

```sh
npx wrangler deploy   # needs a Cloudflare account (wrangler login or CLOUDFLARE_API_TOKEN)
```
