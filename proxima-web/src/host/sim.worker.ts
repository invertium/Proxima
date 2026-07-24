// The authoritative simulation, running off the main thread.
//
// Rendering and UI work cannot stall the game loop, and the loop cannot stall the
// frame — the two only ever exchange structured-cloneable snapshots.

import { TICK_DT } from '../sim/data';
import { createWorld, queueCommand, snapshot, step } from '../sim/world';
import type { ServerMessage, WorkerMessage } from '../net/protocol';
import type { World } from '../sim/types';

let world: World | null = null;
let paused = false;
let accumulator = 0;
let last = performance.now();

const post = (msg: ServerMessage): void => self.postMessage(msg);

self.onmessage = (ev: MessageEvent<WorkerMessage>) => {
  const msg = ev.data;

  if (msg.m === 'boot') {
    world = createWorld({ seed: msg.seed });
    last = performance.now();
    accumulator = 0;
  } else if (msg.m === 'cmd' && world) {
    queueCommand(world, msg.cmd);
  } else if (msg.m === 'pause') {
    paused = msg.paused;
    // Drop whatever piled up while suspended rather than fast-forwarding through it.
    if (!paused) last = performance.now();
  }
};

/**
 * Fixed-step accumulator. The sim only ever advances in whole TICK_DT steps, which is
 * what keeps it deterministic and replayable regardless of the host's frame rate.
 * The catch-up is capped so a backgrounded tab doesn't return and simulate a spiral.
 */
const loop = (): void => {
  const now = performance.now();
  const elapsed = Math.min((now - last) / 1000, 0.25);
  last = now;

  if (world && !paused) {
    accumulator += elapsed;

    let steps = 0;
    while (accumulator >= TICK_DT && steps < 8) {
      step(world, TICK_DT);
      accumulator -= TICK_DT;
      steps += 1;

      // Events are per-tick and must not be coalesced away, so each stepped tick
      // ships its own payload.
      post({ m: 'state', snapshot: snapshot(world), events: [...world.events] });
    }
  }

  setTimeout(loop, 4);
};

loop();
