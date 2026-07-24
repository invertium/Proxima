// The relay: a dumb pipe between the one authoritative host browser and N crew
// stations. It holds no game state, runs no simulation, and validates nothing about
// gameplay — the host does all of that.
//
// Mounted onto the Vite dev server in development (see vite.config.ts) and runnable
// standalone for a packaged build: `node --experimental-strip-types server/relay.ts`.

import { WebSocketServer, WebSocket } from 'ws';
import type { Server } from 'node:http';

export const RELAY_PATH = '/__relay';

export const attachRelay = (server: Server): WebSocketServer => {
  const wss = new WebSocketServer({ noServer: true });

  /** At most one host; whoever connects last wins, so a reloaded host tab takes over. */
  let host: WebSocket | null = null;
  const stations = new Set<WebSocket>();

  server.on('upgrade', (req, socket, head) => {
    const url = new URL(req.url ?? '/', 'http://localhost');
    if (url.pathname !== RELAY_PATH) return;

    wss.handleUpgrade(req, socket, head, (ws) => {
      const role = url.searchParams.get('role') === 'host' ? 'host' : 'station';

      if (role === 'host') {
        host?.close();
        host = ws;
        console.log('[relay] host connected');
      } else {
        stations.add(ws);
        console.log(`[relay] station connected (${stations.size} total)`);
      }

      ws.on('message', (raw) => {
        const data = raw.toString();
        if (role === 'host') {
          // Authoritative state out to every station.
          for (const s of stations) {
            if (s.readyState === WebSocket.OPEN) s.send(data);
          }
        } else if (host?.readyState === WebSocket.OPEN) {
          // Station commands in. The host validates them; the relay does not care.
          host.send(data);
        }
      });

      ws.on('close', () => {
        if (role === 'host') {
          if (host === ws) host = null;
          console.log('[relay] host disconnected');
        } else {
          stations.delete(ws);
        }
      });
    });
  });

  return wss;
};

// Standalone mode: serve dist/ and the relay on one port, which is what a packaged
// build or a LAN game night actually needs.
if (process.argv[1]?.endsWith('relay.ts')) {
  const { createServer } = await import('node:http');
  const { createReadStream, existsSync, statSync } = await import('node:fs');
  const { join, extname, resolve } = await import('node:path');

  const root = resolve(import.meta.dirname, '..', 'dist');
  const port = Number(process.env.PORT ?? 8080);
  const types: Record<string, string> = {
    '.html': 'text/html',
    '.js': 'text/javascript',
    '.css': 'text/css',
    '.glb': 'model/gltf-binary',
    '.json': 'application/json',
    '.webp': 'image/webp',
  };

  const server = createServer((req, res) => {
    const path = (req.url ?? '/').split('?')[0]!;
    let file = join(root, path === '/' ? 'index.html' : path);
    if (!existsSync(file) || statSync(file).isDirectory()) file = join(root, 'index.html');

    res.writeHead(200, { 'content-type': types[extname(file)] ?? 'application/octet-stream' });
    createReadStream(file).pipe(res);
  });

  attachRelay(server);
  server.listen(port, () => {
    console.log(`[relay] Proxima on http://0.0.0.0:${port} — crew joins at /station.html`);
  });
}
