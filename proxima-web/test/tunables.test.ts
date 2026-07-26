// Every gameplay number must either match the C++ build or be a declared decision.
//
// data.ts claimed all of them were "ported verbatim". About twenty weren't, and the
// drift was invisible: you could not tell a balance choice from a typo, and the replay
// tests silently baked in whichever it was. This makes that state unreachable.

import { describe, expect, it } from 'vitest';
import * as data from '../src/sim/data';
import { CPP_REFERENCE, DIVERGENCES } from '../src/sim/data';

type Key = keyof typeof CPP_REFERENCE;

const declared = new Map(DIVERGENCES.map((d) => [d.key as string, d]));

describe('tunables match the C++ build', () => {
  it.each(Object.keys(CPP_REFERENCE) as Key[])('%s', (key) => {
    const live = (data as Record<string, unknown>)[key];
    expect(live, `${key} is in CPP_REFERENCE but not exported from data.ts`).toBeTypeOf('number');

    const divergence = declared.get(key);
    if (!divergence) {
      expect(
        live,
        `${key} differs from the C++ value. Either fix it, or add it to DIVERGENCES with a reason.`,
      ).toBe(CPP_REFERENCE[key]);
      return;
    }

    // A declared divergence must still be honest about both ends.
    expect(divergence.cpp, `DIVERGENCES lists the wrong C++ value for ${key}`).toBe(
      CPP_REFERENCE[key],
    );
    expect(live, `DIVERGENCES says ${key} is ${divergence.web} but data.ts exports ${live}`).toBe(
      divergence.web,
    );
  });
});

describe('the divergence table stays honest', () => {
  it('every declared divergence actually differs', () => {
    for (const d of DIVERGENCES) {
      expect(d.web, `${d.key} is declared as a divergence but matches the C++ value`).not.toBe(d.cpp);
    }
  });

  it('every declared divergence carries a reason', () => {
    for (const d of DIVERGENCES) {
      expect(d.why.length, `${d.key} has no justification`).toBeGreaterThan(30);
    }
  });

  it('every declared divergence names a real tunable', () => {
    for (const d of DIVERGENCES) {
      expect(CPP_REFERENCE[d.key as Key], `${d.key} is not in CPP_REFERENCE`).toBeTypeOf('number');
    }
  });
});
