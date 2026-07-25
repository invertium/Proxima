// Transport between the authoritative host and the crew stations.
//
// The host is authoritative in every implementation: stations submit commands and
// receive snapshots, never the reverse. The relay is a dumb pipe — it never inspects
// or simulates anything, so swapping it for WebRTC data channels later is a change of
// class, not of architecture.
//
// Today: a WebSocket relay (server/relay.ts), which is what makes phones on the LAN
// work. The room is implicit — one host, N stations — which matches how the game is
// actually played.
//
// Next (issue #16): WebRTC data channels, with this relay demoted to signaling. The
// interfaces below are the seam that lands on.

import type { ClientMessage, ServerMessage } from './protocol';

export interface HostTransport {
  broadcast(msg: ServerMessage): void;
  onMessage(handler: (msg: ClientMessage) => void): void;
  /** Fires when another pilot tab takes the host role; this one becomes a spectator. */
  onEvicted(handler: () => void): void;
  /** Number of crew stations currently connected. */
  onCrewCount(handler: (count: number) => void): void;
  close(): void;
}

export interface StationTransport {
  send(msg: ClientMessage): void;
  onMessage(handler: (msg: ServerMessage) => void): void;
  onStatus(handler: (connected: boolean) => void): void;
  close(): void;
}

/** The relay listens on the page's own host, so a phone hitting the LAN IP just works. */
export const relayUrl = (role: 'host' | 'station'): string => {
  const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
  return `${proto}//${location.host}/__relay?role=${role}`;
};

/** Shared reconnect behaviour — a crew phone that sleeps must come back on its own. */
class Socket {
  private ws: WebSocket | null = null;
  private closed = false;
  private retry = 0;

  constructor(
    private readonly url: string,
    private readonly onData: (data: unknown) => void,
    private readonly onStatusChange: (connected: boolean) => void = () => {},
  ) {
    this.connect();
  }

  private connect(): void {
    if (this.closed) return;

    const ws = new WebSocket(this.url);
    this.ws = ws;

    ws.onopen = () => {
      this.retry = 0;
      this.onStatusChange(true);
    };
    ws.onmessage = (ev) => {
      try {
        this.onData(JSON.parse(ev.data as string));
      } catch {
        // A malformed frame is the relay's problem, not the game's — drop it.
      }
    };
    ws.onclose = () => {
      this.onStatusChange(false);
      if (this.closed) return;
      // Back off to a couple of seconds, so a host that's gone doesn't spin the phone's radio.
      this.retry = Math.min(this.retry + 1, 8);
      setTimeout(() => this.connect(), 250 * this.retry);
    };
    ws.onerror = () => ws.close();
  }

  /** Stops reconnecting for good. Used when the relay says we were replaced. */
  latch(): void {
    this.closed = true;
    this.ws?.close();
  }

  send(value: unknown): void {
    if (this.ws?.readyState === WebSocket.OPEN) this.ws.send(JSON.stringify(value));
  }

  close(): void {
    this.closed = true;
    this.ws?.close();
  }
}

export class RelayHost implements HostTransport {
  private handler: (msg: ClientMessage) => void = () => {};
  private evicted: () => void = () => {};
  private crew: (count: number) => void = () => {};

  private readonly sock = new Socket(relayUrl('host'), (data) => {
    // Two relay-level messages the host cares about, neither of which is game state.
    const msg = data as { m?: string; count?: number };
    if (msg.m === 'evicted') {
      // Another pilot tab took over. Stop reconnecting — otherwise the two tabs evict
      // each other in a loop and every station sees interleaved snapshots.
      this.sock.latch();
      this.evicted();
      return;
    }
    if (msg.m === 'crew') {
      this.crew(msg.count ?? 0);
      return;
    }
    this.handler(data as ClientMessage);
  });

  broadcast(msg: ServerMessage): void {
    this.sock.send(msg);
  }

  onMessage(handler: (msg: ClientMessage) => void): void {
    this.handler = handler;
  }

  onEvicted(handler: () => void): void {
    this.evicted = handler;
  }

  onCrewCount(handler: (count: number) => void): void {
    this.crew = handler;
  }

  close(): void {
    this.sock.close();
  }
}

export class RelayStation implements StationTransport {
  private handler: (msg: ServerMessage) => void = () => {};
  private status: (connected: boolean) => void = () => {};
  private readonly sock = new Socket(
    relayUrl('station'),
    (data) => this.handler(data as ServerMessage),
    (connected) => this.status(connected),
  );

  send(msg: ClientMessage): void {
    this.sock.send(msg);
  }

  onMessage(handler: (msg: ServerMessage) => void): void {
    this.handler = handler;
  }

  onStatus(handler: (connected: boolean) => void): void {
    this.status = handler;
  }

  close(): void {
    this.sock.close();
  }
}
