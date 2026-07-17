# Release CI/CD

`.github/workflows/release.yml` packages **Proxima** for **Linux** and **Windows** and publishes a
GitHub Release whenever a version tag is pushed. Builds run inside Epic's prebuilt Unreal Engine
container images (no engine install on the runner).

```
git tag v1.0.0
git push origin v1.0.0      # → packages Linux + Windows → publishes the Release for v1.0.0
```

## Why no macOS

Apple's licensing ties Unreal Engine macOS packaging to a physical Mac; it can't run in a Linux/Windows
cloud container. macOS releases need a **self-hosted Mac runner** with UE 5.7 installed (a job for that
can be added later — the build command is the same `RunUAT BuildCookRun -platform=Mac`).

## One-time setup

The Epic UE images (`ghcr.io/epicgames/unreal-engine`) are private to the EpicGames GitHub org, so CI
must authenticate to pull them.

1. **Link Epic ↔ GitHub.** Sign in at [epicgames.com](https://www.epicgames.com/) → Account →
   **Connected Accounts** → connect GitHub, then accept the invitation to the **EpicGames** GitHub org
   (check email / <https://github.com/orgs/EpicGames/invitation>). This also requires accepting the
   Unreal Engine EULA. Confirm you can see the package at
   <https://github.com/orgs/EpicGames/packages>.
2. **Create a PAT.** GitHub → Settings → Developer settings → **Personal access tokens (classic)** → new
   token with the **`read:packages`** scope.
3. **Add repo secrets** (repo → Settings → Secrets and variables → Actions → *Secrets*):
   | Secret | Value |
   | --- | --- |
   | `EPIC_GHCR_USERNAME` | your GitHub username |
   | `EPIC_GHCR_TOKEN` | the `read:packages` PAT from step 2 |

That's it — push a `v*` tag and the pipeline runs.

## Optional repo variables

Set under *Variables* (not Secrets) to override defaults without editing the workflow:

| Variable | Default | Purpose |
| --- | --- | --- |
| `UE_IMAGE_TAG` | `dev-slim-5.7` | Which UE image tag to pull. Pin to an exact minor (e.g. `dev-slim-5.7.0`) if the rolling tag moves. `dev-slim` = build/cook toolchain without debug symbols (smaller); use `dev-*` if you need the extra tooling. |
| `UE_ROOT_LINUX` | `/home/ue4/UnrealEngine` | Engine path inside the Linux image. |
| `UE_ROOT_WIN` | `C:\UnrealEngine` | Engine path inside the Windows image. |

If the engine path is wrong for the image you pull, the build step fails immediately at `RunUAT` — check
the image's actual layout (`docker run --rm <image> ls /`) and set the matching variable.

## Jobs

| Job | Runner | Notes |
| --- | --- | --- |
| `linux` | `ubuntu-22.04` + Linux UE container | Robust path. Produces `Proxima-<tag>-Linux.tar.gz`. |
| `windows` | `windows-2022` + Windows UE container | **Fragile** (see below). Produces `Proxima-<tag>-Windows.zip`. |
| `release` | `ubuntu-latest` | Attaches whatever packaged successfully to the Release. Publishes as long as **Linux** succeeded — a Windows failure won't sink the release. |

Each packager runs:

```
RunUAT BuildCookRun -project=SpaceGame.uproject -noP4 -utf8output -unattended \
  -nocompile -nocompileeditor -platform=<Linux|Win64> -clientconfig=Shipping \
  -cook -build -stage -pak -compressed -archive -archivedirectory=dist
```

The editor-only plugins (VibeUE, UnrealClaude, ModelingToolsEditorMode) are `TargetAllowList: [Editor]`,
so they're excluded from the Shipping client automatically — no CI changes needed for them.

## The Windows caveat (read this if the Windows job fails)

Windows containers are the weak spot of cloud UE CI:

- **Size.** Epic's Windows image is tens of GB. GitHub-hosted `windows-2022` runners often don't have
  enough free disk to pull it, and the pull is slow.
- **OS matching.** Windows process-isolated containers require the image base OS to match the host
  (`windows-2022` ↔ `ltsc2022`). A mismatched image simply won't start.

If the Windows job fails on image pull, disk, or container start, the two reliable fixes are:

1. **A GitHub Larger Runner** (more disk) for the `windows` job, or
2. **A self-hosted Windows runner** with UE 5.7 installed — then change that job to
   `runs-on: [self-hosted, Windows]`, drop the `container:` block, and point `RunUAT.bat` at the local
   engine. This is the same model you'd use for a macOS runner.

The Linux job is independent and unaffected by any of this.

## Local packaging (reference)

The same build the CI runs, against the local Installed engine:

```
$UE/Engine/Build/BatchFiles/RunUAT.sh BuildCookRun \
  -project="$PWD/SpaceGame.uproject" -noP4 -platform=Linux -clientconfig=Shipping \
  -cook -build -stage -pak -compressed -archive -archivedirectory="$PWD/dist"
```
