// The authoritative simulation, running off the main thread.
//
// Rendering and UI work cannot stall the game loop, and the loop cannot stall the
// frame — the two only ever exchange structured-cloneable snapshots.

import { TICK_DT } from '../sim/data';
import { applySave, toSave } from '../sim/save';
import { createWorld, queueCommand, snapshot, step } from '../sim/world';
import type { ServerMessage, WorkerMessage } from '../net/protocol';
import type { World } from '../sim/types';

let world: World | null = null;
let paused = false;
/** Last save posted, so progress is only persisted when it actually changes. */
let lastSave = '';
let sinceSaveCheck = 0;
let accumulator = 0;
let last = performance.now();

const post = (msg: ServerMessage): void => self.postMessage(msg);

self.onmessage = (ev: MessageEvent<WorkerMessage>) => {
  const msg = ev.data;

  if (msg.m === 'boot') {
    const o = msg.options;
    world = createWorld({
      seed: o.save?.seed ?? o.seed,
      difficulty: o.save?.difficulty ?? o.difficulty,
      shipType: o.save?.shipType ?? o.shipType,
      mode: o.mode,
    });
    if (o.save) applySave(world, o.save);
    lastSave = '';
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

    // Campaign progress is checked twice a second rather than every tick: it changes
    // rarely, and serialising it at 60Hz would be pure waste.
    sinceSaveCheck += elapsed;
    if (world.mode === 'campaign' && sinceSaveCheck > 0.5) {
      sinceSaveCheck = 0;
      const save = toSave(world);
      const encoded = JSON.stringify(save);
      if (encoded !== lastSave) {
        lastSave = encoded;
        post({ m: 'save', save });
      }
    }
  }

  setTimeout(loop, 4);
};

loop();
