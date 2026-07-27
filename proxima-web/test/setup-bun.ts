// Preloaded by `bun test` (see bunfig.toml). Vitest does NOT use this file — it gets a
// DOM from the `environmentMatchGlobs` entry in vite.config.ts instead.
//
// Why it exists: this repo has two test runners that both answer to something spelled
// "test". `bun run test` is vitest, the real suite. `bun test` is Bun's own runner,
// which globs the same *.test.ts files but boots them in a bare V8 with no `document`
// — so the jsdom panel tests exploded with `ReferenceError: document is not defined`
// while the vitest run went green. Two runners disagreeing about whether the console
// works is worse than having one, so Bun's gets a DOM too.
//
// The panel tests are the whole reason the console is trusted; whichever way a
// contributor types "test", they must run.

import { GlobalRegistrator } from '@happy-dom/global-registrator';

if (typeof document === 'undefined') {
  GlobalRegistrator.register();
}
