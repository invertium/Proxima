# proxima-join

Crew-join tool for **Proxima** (issue #10). Lets a crew reach a running ship without typing an IP:
the host registers its address under a **short join code**, the crew type that code at
**https://proxima-join.vercel.app**, and they're forwarded to the ship's bridge consoles.

Live at **https://proxima-join.vercel.app** (Vercel project `proxima-join`).

## What it does

| Route | Purpose |
| --- | --- |
| `/` (`index.html`) | Code-first join page. Enter a join code → resolves + redirects. `<details>` fallback: host IP + PIN. |
| `POST /api/register` | `{host,pin}` → `{ok, code, stateless, kv, ttl, url}`. The game calls this to get a code. |
| `GET /api/resolve?code=` (or `/j/:code`) | Looks the code up and `307`-redirects to the ship, or a friendly 404. |
| `GET /api/join?host=&pin=` | Stateless `307` straight to `http://<ip:port>/stations?pin=<pin>` (no code needed). |
| `GET /api/whoami` | Echoes the caller's public IP (for hosts opening ports to the internet). |
| `POST /api/mcp` | Streamable-HTTP **MCP** endpoint. Tools: `crew_join_url`, `register_host`, `resolve_join_code`. |

## Security: IP-only redirector

Both the URL builder (`lib/crewurl.js`) and the resolve guard (`lib/store.js` `looksLikeCrewUrl`) accept
**only IPv4/IPv6 literals** as the host, and only the fixed `/stations?pin=` path. It can never forward to
an arbitrary hostname, so it can't be abused as an open redirector for phishing. `evil.com` → rejected.

## Join codes: KV vs. stateless

`registerUrl` stores the (validated) crew URL and returns a code:

- **With KV attached** — a short 5-char code (`ABCDEFGHJKMNPQRSTUVWXYZ23456789`, no ambiguous chars),
  stored in Redis with a 6-hour TTL.
- **Without KV** — a longer self-encoding `~<base64url>` code (no storage needed). The tool works either
  way; it upgrades to short codes automatically the moment KV is connected.

`lib/store.js` reads whichever env-var pair Vercel injects:
`KV_REST_API_URL`/`KV_REST_API_TOKEN` **or** `UPSTASH_REDIS_REST_URL`/`UPSTASH_REDIS_REST_TOKEN`.

### Attaching the KV store (one-time, dashboard)

1. Vercel dashboard → **proxima-join** project → **Storage** → **Connect Store**.
2. Pick **Upstash** (Serverless DB — Redis) → create/connect a Redis database.
3. Vercel auto-injects the REST env vars. Redeploy (or deploy again) to pick them up.

Currently **attached** — the registry serves short codes.

## Enabling it in the game

The game side is **opt-in** (it posts the host's LAN IP + PIN to this service):

- **In-game:** Settings → **ONLINE JOIN CODE — ON**. Persists to `GameUserSettings.ini`
  (`[SpaceGame.Online] EnableJoinCode`).
- **Console (equivalent):** `sg.OnlineJoinCode 1`. The registration URL is `sg.OnlineJoinUrl`
  (defaults to `https://proxima-join.vercel.app/api/register`).

When on, `UStationServerSubsystem::RequestJoinCode` POSTs `{host,pin}` on server bind, caches the returned
code, and shows it on `/api/state` (`joinCode`) for the ship's screen.

## Deploying

Zero-config: static `index.html` at root + `api/*.js` serverless functions, no build step. Deployed via the
Vercel MCP (`deploy_to_vercel`, `target:"production"`, `name:"proxima-join"`) or `vercel --prod`.
