# SpaceGame — packaging recipes (Unreal 5.7 BuildCookRun via UAT)
#
# Cross-compile reality: a host can only package for some targets.
#   Linux host  -> Linux only
#   Windows host-> Windows and Linux
#   macOS host  -> macOS only          (Apple toolchain / signing is Mac-locked)
# So `package-windows` / `package-mac` are here for use ON those hosts; they
# cannot run from Linux. Editor-only plugins (VibeUE, UnrealClaude,
# ModelingToolsEditorMode) are TargetAllowList=Editor and drop out of packages.
#
# The editor MUST be closed while packaging (UBT build mutex).

# --- Tunables (override on the CLI, e.g. `just config=Development package-mac`) ---
# Override the engine location with `UE_ROOT=/path just …`; the defaults match a stock install.
engine     := env_var_or_default("UE_ROOT", if os() == "macos" { "/Users/Shared/Epic Games/UE_5.7" } else { env_var("HOME") / "UnrealEngine/UE_5.7" })
project    := justfile_directory() / "SpaceGame.uproject"
archive    := justfile_directory() / "Packaged"
config     := "Shipping"                 # Shipping | Development | Debug
maps       := "/Game/Maps/MainMenu+/Game/Maps/VSlice_Arena"

uat        := engine / "Engine/Build/BatchFiles/RunUAT.sh"
build      := engine / "Engine/Build/BatchFiles" / if os() == "macos" { "Mac/Build.sh" } else { "Linux/Build.sh" }
host       := if os() == "macos" { "Mac" } else { "Linux" }
_flags     := "-noP4 -build -cook -stage -pak -package -archive -unattended -nocompileeditor"

# Default: list recipes
default:
    @just --list

# Compile the editor target for the current host platform.
build-editor:
    "{{build}}" SpaceGameEditor {{host}} Development -Project="{{project}}" -WaitMutex

# Package a standalone Linux build -> Packaged/Linux/  (run ./SpaceGame.sh)
package-linux:
    "{{uat}}" BuildCookRun -project="{{project}}" -platform=Linux \
        -clientconfig={{config}} -map="{{maps}}" \
        -archivedirectory="{{archive}}" {{_flags}}

# Package a standalone Windows build. MUST run on a Windows host (RunUAT.bat).
package-windows:
    "{{uat}}" BuildCookRun -project="{{project}}" -platform=Win64 \
        -clientconfig={{config}} -map="{{maps}}" \
        -archivedirectory="{{archive}}" {{_flags}}

# Package a standalone macOS build. MUST run on a macOS host.
package-mac:
    "{{uat}}" BuildCookRun -project="{{project}}" -platform=Mac \
        -clientconfig={{config}} -map="{{maps}}" \
        -archivedirectory="{{archive}}" {{_flags}}

# Launch the packaged Linux build (after `just package-linux`)
run-linux:
    "{{archive}}/Linux/SpaceGame.sh"

# Delete packaged output
clean:
    rm -rf "{{archive}}"
