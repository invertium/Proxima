import { defineConfig, type PluginOption } from 'vite';
import { resolve } from 'node:path';
import { attachRelay } from './server/relay';

// The crew relay rides on the dev server, so `npm run dev` is the whole setup: one
// port, one URL to hand round the room, no second process to remember.
const relayPlugin = (): PluginOption => ({
  name: 'proxima-relay',
  configureServer(server) {
    if (server.httpServer) attachRelay(server.httpServer);
  },
});

// Two entry points: the host/pilot view (index.html) and the crew console
// (station.html). Both ship from the same static build — see README §Distribution.
export default defineConfig({
  plugins: [relayPlugin()],
  build: {
    target: 'es2022',
    rollupOptions: {
      input: {
        host: resolve(__dirname, 'index.html'),
        station: resolve(__dirname, 'station.html'),
      },
    },
  },
  // strictPort: silently sliding to 5174 when 5173 is busy means the docs, the tests
  // and the human all end up talking to different servers.
  server: { host: true, port: 5173, strictPort: true },
});
