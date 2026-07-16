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
- Project root: `~/spaceGame`
- Living state in `docs/`: PROJECT_PLAN.md, PROGRESS.md, DECISIONS.md. Read these first every session.

### D4 — AI tooling
- **UnrealClaude**: viewport screenshots + actor control over MCP.
- **VibeUE**: Blueprint editing + Python API over MCP (needs free API key from vibeue.com).
- Both expose MCP servers; used together (UnrealClaude = see/move, VibeUE = build/script).

### D6 — Game definition (Phase 1, 2026-06-18)
- **Genre:** Spaceship **bridge simulator**, à la Artemis / EmptyEpsilon — one ship crewed across specialized stations.
- **Scope:** **Vertical slice** = one polished combat encounter loop.
- **Stations in slice:** **Helm + Weapons + Engineering** (Science, Comms/Relay later).
- **Multiplayer:** **single-machine first** — full sim + station UIs run locally; client/server multiplayer is a dedicated LATER phase.
- **Presentation:** stylized **3D third-person** "main screen" (game viewport, follow cam) + **2D sci-fi UMG** station consoles with a tactical radar.

### D5 — Blueprint vs C++ split (finalized)
- **C++** = simulation core: `ASpaceship`, subsystem `UActorComponent`s (power/movement/weapon/health), enemy AI, damage. Authored directly (Edit + `Build.sh`, proven working), verified via `UE_LOG` + PIE behavior.
- **Blueprint/UMG** = station consoles, radar, HUD, FX, level glue, spawning. Authored via VibeUE, verified via screenshots.
- **Why:** sim is stateful and must be **replication-ready** for the networking phase; direct C++ authoring is more reliable for me than MCP-driven node-graph construction (UnrealClaude `set_property` proved flaky); UI iterates faster in BP and reads clearly in still screenshots.

### D7 — Networking deferred, but design replication-ready
- Keep authoritative **sim state separate from presentation**; ship/subsystem state as `UPROPERTY`s structured for future replication. Single-machine slice = one world, stations = local UMG overlays. Goal: networking phase is an additive layer, not a rewrite.

### D8 — Folder structure
- C++: `Source/SpaceGame/{Core, Ships, Components, AI}`
- Content: `/Game/{Maps, Ships, UI/Stations, UI/Common, FX, Materials}`
- Slice map: `/Game/Maps/VSlice_Arena`

### D9 — Asset strategy
- **Primitive-first**: engine basic shapes + stylized emissive materials + Niagara FX; add Starter Content for materials/particles. Replace placeholders with low-poly ship models later. **No hard external-art dependency for the slice.**

### D10 — Station model (single-machine)
- `StationManager` switches the active console via number keys (1=Helm, 2=Weapons, 3=Engineering). Main screen = game viewport (3rd-person follow cam). Consoles = full-screen UMG overlays. (Multi-window / per-client comes with networking.)

### D11 — Power model (Engineering)
- EmptyEpsilon-like per-system power levels (0..max) that scale performance: impulse power→max speed, beam power→recharge rate, shield power→damage mitigation. Heat/coolant optional, deferred. Keep minimal in slice.
