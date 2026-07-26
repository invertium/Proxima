// The contract every crew console panel implements.
//
// `root` is built once in the create* function and never replaced; `update` may only
// write text, attributes and classes. That invariant is what keeps a button alive
// under a finger for the whole duration of a press.

import type { Command, Snapshot } from '../../sim/types';

export interface Panel {
  root: HTMLElement;
  update(s: Snapshot): void;
}

/** Panels send commands; they never touch the transport directly. */
export type Send = (cmd: Command) => void;

export const km = (v: number): string => `${(v / 1000).toFixed(1)} km`;
export const pct = (v: number, max: number): string =>
  `${Math.round((max > 0 ? v / max : 0) * 100)}%`;
