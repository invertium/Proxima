// Wire format shared by the host, the sim worker, and the crew stations.
//
// The same message types cross all three boundaries (worker postMessage, and the
// station transport), so there is exactly one definition of what a command and a
// state update look like — the protocol drift the C++/JSON/embedded-HTML split
// invited can't happen here.

import type { SaveGame } from '../sim/save';
import type { Command, Difficulty, GameMode, PlayerShipType, SimEvent, Snapshot } from '../sim/types';

export const PROTOCOL_VERSION = 1;

/** Host -> sim worker, and host -> crew stations. */
export type ServerMessage =
  | { m: 'state'; snapshot: Snapshot; events: SimEvent[] }
  /** Emitted when campaign progress changes, for the host to persist. */
  | { m: 'save'; save: SaveGame }
  /** Refused commands, broadcast to all stations; each filters its own by id. */
  | { m: 'ack'; acks: { id?: number; ok: boolean; reason?: string }[] }
  | { m: 'hello'; version: number; station: string }
  | { m: 'error'; reason: string };

/** Crew station -> host, and host -> sim worker. */
export type ClientMessage =
  /** `id` lets the sender match a refusal back to the control that was pressed. */
  | { m: 'cmd'; cmd: Command; id?: number }
  | { m: 'join'; station: string; version: number }
  /**
   * Session control from a crew console. Deliberately NOT a Command: the sim cannot
   * cleanly reboot itself, so restarting is the host's job. Keeping Command for
   * in-world actions only is a boundary worth defending.
   */
  | { m: 'game'; action: 'restart' | 'new' };

/** Sim worker control messages. */
export interface BootOptions {
  seed: number;
  difficulty: Difficulty;
  shipType: PlayerShipType;
  mode: GameMode;
  /** Resume from this save instead of starting fresh. */
  save?: SaveGame | null;
}

export type WorkerMessage =
  | { m: 'boot'; options: BootOptions }
  | { m: 'cmd'; cmd: Command; id?: number }
  | { m: 'pause'; paused: boolean };
