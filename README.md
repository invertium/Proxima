# SpaceGame — a co-op starship bridge simulator

An Artemis / EmptyEpsilon-style bridge simulator built in **Unreal Engine 5.7** (C++) on Linux.
One player pilots from a 3rd-person view of the ship; the rest of the "crew" open **web consoles**
on their phones/laptops over the LAN (Helm, Weapons, Engineering, Science) and run the stations
together. It ships with a small story campaign across an open sector — fly between planets and a
sun, trigger encounters by proximity, warp to travel, dock to repair and buy upgrades/ships.

> This is a source project (no packaged build committed). You compile the editor target and play
> in-editor (PIE). The steps below take you from a clean checkout to a running game.

---

## 1. Prerequisites

| Requirement | Notes |
|---|---|
| **Unreal Engine 5.7** (source or Linux binary build) | Tested against **5.7.4** installed at `~/UnrealEngine/UE_5.7`. |
| **Linux** | Developed on CachyOS / Arch (kernel 7.x), Wayland + Vulkan. Any modern Linux with a Vulkan-capable GPU should work. |
| **Clang toolchain** | The one bundled with UE 5.7 (`Engine/Extras/ThirdPartyNotUE/SDKs/...`) — no separate install needed. |
| **~150 GB disk + a discrete GPU** | Standard UE editor requirements. |
| **Python 3** | Only for the optional agent/automation tooling in `tools/`. Not needed to play. |

**Find your engine path.** This repo assumes `~/UnrealEngine/UE_5.7`. Your install is recorded in
`~/.config/Epic/UnrealEngine/Install.ini`:

```ini
[Installations]
UE_5.7=/home/<you>/UnrealEngine/UE_5.7
```

If yours differs, substitute your path for `$UE` below.

```fish
# set once per shell (fish syntax; use `export` for bash)
set -x UE $HOME/UnrealEngine/UE_5.7
set -x PROJ (pwd)/SpaceGame.uproject   # run from the repo root
```

---

## 2. What's in the box

- **Game module:** `Source/SpaceGame/` — the C++ runtime (Ships, Components, Core, FX, Net, World).
- **Targets:** `Source/SpaceGame.Target.cs` (game) and `Source/SpaceGameEditor.Target.cs` (editor).
- **Content:** `Content/` — maps (`Maps/MainMenu`, `Maps/VSlice_Arena`), art, materials, audio.
- **Plugins:** `Plugins/UnrealClaude` and `Plugins/VibeUE` — **AI-agent dev tooling only** (MCP
  servers used to drive the editor during development). They are **not required to play**; you may
  disable them in `SpaceGame.uproject` if you don't want them.
- The web consoles are hosted **in-process by the game itself** (`Net/StationServerSubsystem`,
  HTTP on port **8080**) — there is no separate web server to run.

Build artifacts (`Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`) are gitignored and
regenerate on first build.

---

## 3. Build

Editor closed, from anywhere:

```fish
$UE/Engine/Build/BatchFiles/Linux/Build.sh \
    SpaceGameEditor Linux Development \
    -Project=$PROJ -WaitMutex
```

- First build is slow (compiles the game module + the two plugins). Subsequent builds are incremental.
- `-WaitMutex` avoids clobbering a build the editor might be running. If a build exits `1`
  immediately after killing an editor, it's usually mutex contention — just run it again.
- No separate "generate project files" step is required to build from the command line. (For an
  IDE, run `$UE/Engine/Build/BatchFiles/Linux/GenerateProjectFiles.sh -project=$PROJ -game`.)

---

## 4. Run the game (editor + Play-In-Editor)

Launch the editor with the project:

```fish
$UE/Engine/Binaries/Linux/UnrealEditor $PROJ
```

It opens on **`Maps/MainMenu`**. Press **Play** (the ▶ toolbar button, or `Alt+P`) to start a
Play-In-Editor session. Use **New Game** from the menu to begin the campaign; you spawn in the open
sector at the home system.

**Pilot controls (Helm / keyboard, in the focused PIE window):**
- Throttle / steer the ship, orient toward contacts.
- `1` / `2` / `3` switch the local on-screen station (Helm / Weapons / Engineering).

You can play solo from the editor window, but the intended experience is the **crew web consoles**
below.

---

## 5. Play with a crew — the LAN web consoles

While a PIE (or packaged) session is running, the game hosts the bridge stations as web pages on
**port 8080**. On any device on the same network, open:

```
http://<the-game-machine-LAN-IP>:8080/
```

From the same machine, `http://localhost:8080/` works too. Each crew member picks a station:

- **Helm** — radar, throttle/steering, firing-arc wedges, **DOCK/UNDOCK**, **WARP**, sector map.
- **Weapons** — target list (enemy callsigns), phaser/torpedo firing (only within the correct arc).
- **Engineering** — power routing / reactor budget, and (while docked at a station) the **drydock**:
  buy tiered upgrades and new ships with earned credits/XP.
- **Science** — comms log, sector starmap.

Find your LAN IP with `ip addr` (look for the `192.168.x.x` / `10.x.x.x` address). The consoles are
mobile-first and scrollable.

---

## 6. The game loop (what to do)

1. **New Game** → you're in the home system (a blue planet, Haven) with a friendly starbase.
2. **Fly toward the next objective** (the highlighted system on the sector map). **Warp** to shorten
   long hops.
3. Entering an objective's zone **spawns the enemy fleet** and opens comms. Orient and destroy them
   (respect the phaser/torpedo firing arcs shown on the radar).
4. Clearing a fleet **advances the campaign** seamlessly and points you at the next system.
5. **Dock at the starbase** to repair, resupply torpedoes, and spend credits/XP on **upgrades or a
   new ship** in Engineering's drydock.
6. Clear the final system (the Pact staging point at the **Ember** sun) for the epilogue. Being
   destroyed shows the defeat screen; retry reloads the sector.

---

## 7. Troubleshooting

- **`Build.sh: No such file`** — your engine isn't at `~/UnrealEngine/UE_5.7`. Check
  `~/.config/Epic/UnrealEngine/Install.ini` and update `$UE`.
- **Build exits `1` right after closing the editor** — mutex contention; re-run the build once.
- **Web consoles don't load** — confirm a PIE/game session is actually running (the server only
  exists while the game is playing), that you're using the machine's LAN IP, and that port `8080`
  isn't firewalled. Check the log for `[StationServer]`.
- **Plugins fail to compile / you don't want the agent tooling** — set `"Enabled": false` for
  `UnrealClaude` and `VibeUE` in `SpaceGame.uproject`, then rebuild. The game itself doesn't depend
  on them.

---

## 8. Repository layout & docs

```
SpaceGame.uproject         project descriptor (module + plugins)
Source/SpaceGame/          C++ game module
  Ships/  Components/  Core/  FX/  Net/  World/
Content/                   maps, art, materials, audio
Plugins/                   UnrealClaude, VibeUE (dev/agent tooling only)
tools/                     Python helpers for editor automation (optional)
docs/
  PROGRESS.md              append-only build log (per feature/commit)
  PROJECT_PLAN.md          phased roadmap + "done-when" checks
  DECISIONS.md             design decisions
```

Development follows a **one-commit-per-feature** cadence; `docs/PROGRESS.md` is the running history
of what was built and how it was verified.

---

## 9. License

Licensed under the **[PolyForm Noncommercial License 1.0.0](LICENSE)** — free to use, modify, and
share for any **noncommercial** purpose (personal, hobby, research, education); **commercial use is
not permitted**. This covers the SpaceGame source, content, and docs. Unreal Engine (UE EULA), the
bundled dev-tooling plugins, and any downloaded art assets keep their own separate licenses.

### A note for contributors / agents

The VibeUE agent tooling uses an API key stored in
`Saved/Config/LinuxEditor/EditorPerProjectUserSettings.ini`, which is **gitignored and must never be
committed**. Nothing there is needed to compile or play the game.
