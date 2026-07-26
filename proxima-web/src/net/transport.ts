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
import type { Snapshot } from '../sim/types';

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
  /** Sends a command tagged with a fresh id, and returns that id. */
  sendCommand(msg: ClientMessage & { m: 'cmd' }): number;
  /** True for ids this page minted, so acks broadcast to every station filter cleanly. */
  ownsId(id: number | undefined): boolean;
  /** The relay refused our PIN. */
  onRejected(handler: () => void): void;
  onSnapshot(handler: (snap: Snapshot) => void): void;
  onMessage(handler: (msg: ServerMessage) => void): void;
  onStatus(handler: (connected: boolean) => void): void;
  close(): void;
}

/** The relay listens on the page's own host, so a phone hitting the LAN IP just works. */
export const relayUrl = (role: 'host' | 'station', pin?: string | null): string => {
  const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
  const query = pin ? `&pin=${encodeURIComponent(pin)}` : '';
  return `${proto}//${location.host}/__relay?role=${role}${query}`;
};

/** Close code the relay uses to say "that PIN was wrong". */
export const BAD_PIN = 4003;

/**
 * The host's session PIN, minted once per browser session. Kept in sessionStorage so a
 * host reload rejoins its own session instead of locking out the crew.
 */
export const sessionPin = (): string => {
  let pin = storedPin();
  if (!pin) {
    pin = String(Math.floor(1000 + Math.random() * 9000));
    sessionStorage.setItem('proxima.pin', pin);
  }
  return pin;
};

/**
 * The PIN this page should join with. A `?pin=` in the URL wins and is remembered, so
 * a crew can be handed a join link (or a QR code) instead of typing four digits on a
 * phone; otherwise whatever they typed last stands.
 */
export const storedPin = (): string | null => {
  const fromUrl = new URLSearchParams(location.search).get('pin');
  if (fromUrl) {
    sessionStorage.setItem('proxima.pin', fromUrl);
    return fromUrl;
  }
  return sessionStorage.getItem('proxima.pin');
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
    private readonly onRejected: () => void = () => {},
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
    ws.onclose = (ev) => {
      this.onStatusChange(false);
      // A wrong PIN is not a transient failure; reconnecting in a loop would just
      // hammer the relay and never succeed.
      if (ev.code === BAD_PIN) {
        this.closed = true;
        this.onRejected();
        return;
      }
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

  private readonly sock = new Socket(relayUrl('host', sessionPin()), (data) => {
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
  private snapshot: (snap: Snapshot) => void = () => {};
  private status: (connected: boolean) => void = () => {};
  private currentlyConnected = false;

  /**
   * Command ids are scoped to this page by a random nonce, so acks broadcast to every
   * station can be filtered to our own with one comparison — and the relay stays a
   * dumb pipe that knows nothing about who sent what.
   */
  private readonly nonce = Math.floor(Math.random() * 1e6) * 1e6;
  private seq = 0;
  private rejected: () => void = () => {};
  private readonly sock = new Socket(
    relayUrl('station', storedPin()),
    (data) => {
      const msg = data as ServerMessage;
      if (msg.m === 'state') this.snapshot(msg.snapshot);
      this.handler(msg);
    },
    (connected) => {
      this.currentlyConnected = connected;
      this.status(connected);
    },
    () => this.rejected(),
  );

  send(msg: ClientMessage): void {
    this.sock.send(msg);
  }

  sendCommand(msg: ClientMessage & { m: 'cmd' }): number {
    const id = this.nonce + this.seq++;
    this.sock.send({ ...msg, id });
    return id;
  }

  ownsId(id: number | undefined): boolean {
    return id !== undefined && id >= this.nonce && id < this.nonce + 1e6;
  }

  onSnapshot(handler: (snap: Snapshot) => void): void {
    this.snapshot = handler;
  }

  onMessage(handler: (msg: ServerMessage) => void): void {
    this.handler = handler;
  }

  onStatus(handler: (connected: boolean) => void): void {
    this.status = handler;
    if (this.currentlyConnected) handler(true);
  }

  /** The relay refused our PIN. The console should ask for one. */
  onRejected(handler: () => void): void {
    this.rejected = handler;
  }

  close(): void {
    this.sock.close();
  }
}
