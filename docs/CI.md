# Release CI/CD

`.github/workflows/release.yml` packages **Proxima** and publishes a GitHub Release whenever a version
tag is pushed. The **Linux** build runs in the cloud inside Epic's prebuilt Unreal Engine container image
(no engine install on the runner). **Windows** requires a self-hosted runner (see below).

```
git tag v1.0.0
git push origin v1.0.0      # → packages Linux (cloud) → publishes the Release for v1.0.0
```

## What runs where — and why

| Platform | How | Status |
| --- | --- | --- |
| **Linux** | Native Linux UE container on a GitHub-hosted `ubuntu` runner | ✅ Cloud, works out of the box |
| **Windows** | Self-hosted Windows runner with UE installed | ⚙️ Dormant until you provide a runner |
| **macOS** | Self-hosted Mac runner with UE installed | ❌ Not implemented |

**Why Windows and macOS can't run in the cloud here:**

- GitHub Actions **cannot run `container:` jobs on Windows or macOS runners** — only on Linux. (The exact
  error is `Container operations are only supported on Linux runners`.) So Epic's Windows UE image can't
  be used as a GitHub-hosted job.
- Unreal **can't cross-compile a Windows or Mac target from a Linux host**, so the Linux container can't
  produce them either.
- macOS additionally requires physical Apple hardware by Apple/Epic licensing.

The result: **Linux is the cloud deliverable**; Windows/macOS need self-hosted runners with UE installed.

> **Status:** the Linux cloud path is verified end-to-end (compile editor + Shipping game → cook → stage →
> pak → archive → publish). Budget **~30 min** per release run. The cook runs as the image's non-root
> `ue4` user — UnrealEditor refuses to cook as root — which the workflow handles automatically.

## One-time setup (Linux / cloud)

The Epic UE images (`ghcr.io/epicgames/unreal-engine`) are private to the EpicGames GitHub org, so CI must
authenticate to pull them.

1. **Link Epic ↔ GitHub.** [epicgames.com](https://www.epicgames.com/account/connections) → connect
   GitHub, accept the invite to the **EpicGames** org (<https://github.com/orgs/EpicGames/invitation>),
   and accept the UE EULA. Confirm the images are visible at
   <https://github.com/orgs/EpicGames/packages>.
2. **Create a classic PAT** with the **`read:packages`** scope
   (<https://github.com/settings/tokens/new>).
3. **Add repo secrets** (repo → Settings → Secrets and variables → Actions → *Secrets*):
   | Secret | Value |
   | --- | --- |
   | `EPIC_GHCR_USERNAME` | your GitHub username |
   | `EPIC_GHCR_TOKEN` | the `read:packages` PAT |

Push a `v*` tag and the Linux release runs.

## Enabling Windows (self-hosted)

The `windows` job is dormant until you opt in, so it never blocks the Linux release.

1. On a Windows machine with **UE 5.7 installed**, register a GitHub **self-hosted runner** (repo →
   Settings → Actions → Runners → New self-hosted runner → Windows) and give it the label `windows`.
2. Set repo **variable** `ENABLE_WINDOWS` = `1`.
3. If UE isn't at the default `C:\Program Files\Epic Games\UE_5.7`, set variable `UE_ROOT_WIN` to its path.

Next `v*` tag then also packages Windows on your runner and attaches `Proxima-<tag>-Windows.zip` to the
Release. macOS works the same way — add a `macos` job on a self-hosted Mac runner with `-platform=Mac`.

## Optional repo variables

| Variable | Default | Purpose |
| --- | --- | --- |
| `UE_IMAGE_TAG` | `dev-slim-5.7` | UE Linux image tag. Pin to an exact minor (e.g. `dev-slim-5.7.0`) if the rolling tag moves. `dev-slim` = build/cook toolchain minus debug symbols. |
| `UE_ROOT_LINUX` | `/home/ue4/UnrealEngine` | Engine path inside the Linux image. |
| `ENABLE_WINDOWS` | *(unset)* | Set to `1` to activate the self-hosted Windows job. |
| `UE_ROOT_WIN` | `C:\Program Files\Epic Games\UE_5.7` | Engine path on the self-hosted Windows runner. |

## The build

Each packager runs the same command (platform differs):

```
RunUAT BuildCookRun -project=SpaceGame.uproject -noP4 -utf8output -unattended \
  -nocompile -nocompileeditor -platform=<Linux|Win64> -clientconfig=Shipping \
  -cook -build -stage -pak -compressed -archive -archivedirectory=dist
```

The editor-only plugins (VibeUE, UnrealClaude, ModelingToolsEditorMode) are `TargetAllowList: [Editor]`,
so they're excluded from the Shipping client automatically — no CI changes needed for them.

The `release` job attaches whatever packaged and publishes the Release **as long as the Linux job
succeeded**, so a missing/failed Windows job never sinks the release.

## Local packaging (reference)

The same build against the local Installed engine:

```
$UE/Engine/Build/BatchFiles/RunUAT.sh BuildCookRun \
  -project="$PWD/SpaceGame.uproject" -noP4 -platform=Linux -clientconfig=Shipping \
  -cook -build -stage -pak -compressed -archive -archivedirectory="$PWD/dist"
```
