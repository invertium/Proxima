# DECISIONS (architecture / tech choices + rationale)

Append decisions; don't re-litigate. Newest at bottom.

---

### D1 — Engine version: UE 5.7
- **Why:** UnrealClaude and VibeUE both officially target 5.7. UE 5.8 released 2026-06-17 but plugin 5.8 compatibility unverified.
- **Revisit when:** both plugins confirm 5.8 support.

### D2 — Platform / RHI: Linux + Vulkan
- **Why:** Host is CachyOS w/ AMD RX 9070 XT, Mesa 26.1.2, Vulkan 1.4. Vulkan is UE's Linux RHI. No NVIDIA/CUDA.
- **Implication:** Screenshot capture is engine-internal (Slate viewport render), not OS screen-grab, so it works as long as the GPU/Vulkan context is live. Real Wayland+XWayland display is present.

### D3 — Project root & docs
- Project root: `/home/julian/gitrepos/spaceGame`
- Living state in `docs/`: PROJECT_PLAN.md, PROGRESS.md, DECISIONS.md. Read these first every session.

### D4 — AI tooling
- **UnrealClaude**: viewport screenshots + actor control over MCP.
- **VibeUE**: Blueprint editing + Python API over MCP (needs free API key from vibeue.com).
- Both expose MCP servers; used together (UnrealClaude = see/move, VibeUE = build/script).

### D5 — Blueprint vs C++ split — TBD (Phase 1)
- Default lean: Blueprint-first for gameplay (drives VibeUE Blueprint tools + screenshot verification loop); C++ only where perf/structure demands. Finalize after game design questions answered.
