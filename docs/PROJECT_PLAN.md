# PROJECT PLAN — phased roadmap

Status legend: [ ] todo · [~] in progress · [x] done · [!] blocked

Each milestone needs a concrete "done when…" check confirmable via screenshot or log/PIE assertion.

---

## Phase 0 — Environment & setup  [~]
- [x] Audit OS/GPU/display/toolchain (see PROGRESS 2026-06-17)
- [x] Install UE 5.7 on Linux — binary 5.7.4 at /home/julian/UnrealEngine/UE_5.7, verified runnable (2026-06-18)
- [x] Create SpaceGame C++ project + clone UnrealClaude & VibeUE into Plugins/ (2026-06-18)
- [x] Build plugins — SpaceGameEditor build Succeeded; all 3 module .so built (patched VibeUE -Wcomment bug)
- [x] Headless load verification — both plugins mount/init, no crash; UnrealClaude registered capture_viewport + actor tools
- [ ] Launch editor GUI (recommend: I launch in background; first launch compiles shaders, may take minutes)
- [ ] Register UnrealClaude MCP (localhost:3000) in this Claude Code session
- [ ] **MCP smoke test** — start PIE on empty level → capture_viewport → read back → stop PIE
- [!] USER (deferred to after smoke test): paste VibeUE API key, register VibeUE MCP (8088)
- [ ] **MCP smoke test** — done when: connect to MCP, list tools, start PIE on empty level, capture 1 viewport screenshot, read it back, stop PIE — all succeed. Highest-risk step; gate before any game work.

## Phase 1 — Game design & plan  [ ]
- [ ] Answer 3 scoping questions (genre, scope, art style, target platform)
- [ ] Finalize PROJECT_PLAN milestones + DECISIONS (BP/C++ split, folders, asset strategy)
- [ ] User approves plan → exit planning mode

## Phase 2+ — Build (defined after Phase 1 approval)
- TBD. Will be broken into tiny, screenshot-verifiable milestones.
