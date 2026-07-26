import { useEffect } from 'react';

import type { Dispatch, MutableRefObject, SetStateAction } from 'react';
import type { Snapshot } from '../sim/types';
import type { SectorView } from '../render/scene';

import { audio, cmd } from './main';

interface Args {
  readonly menuOpenRef: MutableRefObject<boolean>;
  readonly setMenuOpen: Dispatch<SetStateAction<boolean>>;
  readonly snapshotRef: MutableRefObject<Snapshot | null>;
  readonly viewRef: MutableRefObject<SectorView | null>;
}

export function usePilotLoop({ menuOpenRef, setMenuOpen, snapshotRef, viewRef }: Args) {
  useEffect(() => {
    const held = new Set<string>();
    let lastThrottle = 0;
    let lastTurn = 0;
    let lastStrafe = 0;
    let lastFrame = performance.now();
    let frameId = 0;

    const cycleTarget = (): void => {
      const currentSnapshot = snapshotRef.current;
      if (!currentSnapshot || currentSnapshot.contacts.length === 0) return;

      const ids = currentSnapshot.contacts.map((contact) => contact.id);
      const at = ids.indexOf(currentSnapshot.player.targetId ?? -1);
      const nextId = ids[(at + 1) % ids.length];
      if (nextId !== undefined) cmd({ c: 'target', id: nextId });
    };

    const pumpInput = (): void => {
      const throttle = (held.has('KeyW') ? 1 : 0) - (held.has('KeyS') ? 1 : 0);
      const turn = (held.has('KeyD') ? 1 : 0) - (held.has('KeyA') ? 1 : 0);
      const strafe = (held.has('KeyE') ? 1 : 0) - (held.has('KeyQ') ? 1 : 0);

      if (strafe !== lastStrafe) {
        cmd({ c: 'strafe', v: strafe });
        lastStrafe = strafe;
      }
      if (throttle !== lastThrottle) {
        cmd({ c: 'throttle', v: throttle });
        lastThrottle = throttle;
      }
      if (turn !== lastTurn) {
        cmd({ c: 'turn', v: turn });
        lastTurn = turn;
      }
    };

    const onKeyDown = (event: KeyboardEvent): void => {
      audio.unlock();
      if (event.repeat) return;

      held.add(event.code);
      if (event.code === 'Space') cmd({ c: 'fireBeam' });
      if (event.code === 'KeyF') cmd({ c: 'fireTorpedo' });
      if (event.code === 'KeyG') cmd({ c: 'dock' });
      if (event.code === 'KeyJ') cmd({ c: 'warp' });
      if (event.code === 'Enter') cmd({ c: 'acceptObjective' });
      if (event.code === 'KeyR') cmd({ c: 'weld' });
      if (event.code === 'KeyV') cmd({ c: 'alert', state: 'toggle' });

      if (event.code === 'Tab') {
        event.preventDefault();
        cycleTarget();
      }
      if (event.code === 'Escape' && snapshotRef.current) {
        setMenuOpen((current) => !current);
      }
    };

    const onKeyUp = (event: KeyboardEvent): void => {
      held.delete(event.code);
    };

    const onBlur = (): void => {
      held.clear();
    };

    const frame = (): void => {
      frameId = window.requestAnimationFrame(frame);

      const currentSnapshot = snapshotRef.current;
      const view = viewRef.current;
      if (!currentSnapshot || !view) return;

      const now = performance.now();
      const dt = Math.min((now - lastFrame) / 1000, 0.1);
      lastFrame = now;

      if (!menuOpenRef.current) pumpInput();
      view.update(currentSnapshot, dt);
      audio.setThrottle(Math.abs(currentSnapshot.player.speed) / Math.max(1, currentSnapshot.player.maxSpeed));
    };

    window.addEventListener('keydown', onKeyDown);
    window.addEventListener('keyup', onKeyUp);
    window.addEventListener('blur', onBlur);
    frameId = window.requestAnimationFrame(frame);

    return () => {
      window.removeEventListener('keydown', onKeyDown);
      window.removeEventListener('keyup', onKeyUp);
      window.removeEventListener('blur', onBlur);
      window.cancelAnimationFrame(frameId);
    };
  }, [menuOpenRef, setMenuOpen, snapshotRef, viewRef]);
}
