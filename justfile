# SpaceGame — packaging recipes (Unreal 5.7 BuildCookRun via UAT)
#
# Cross-compile reality: a host can only package for some targets.
#   Linux host  -> Linux only          (this machine)
#   Windows host-> Windows and Linux
#   macOS host  -> macOS only          (Apple toolchain / signing is Mac-locked)
# So `package-windows` / `package-mac` are here for use ON those hosts; they
# cannot run from Linux. Editor-only plugins (VibeUE, UnrealClaude,
# ModelingToolsEditorMode) are TargetAllowList=Editor and drop out of packages.
#
# The editor MUST be closed while packaging (UBT build mutex).

# --- Tunables (override on the CLI, e.g. `just config=Development package-linux`) ---
engine     := "/home/julian/UnrealEngine/UE_5.7"
project    := justfile_directory() / "SpaceGame.uproject"
archive    := justfile_directory() / "Packaged"
config     := "Shipping"                 # Shipping | Development | Debug
maps       := "/Game/Maps/MainMenu+/Game/Maps/VSlice_Arena"

uat        := engine / "Engine/Build/BatchFiles/RunUAT.sh"
_flags     := "-noP4 -build -cook -stage -pak -archive -unattended -nocompileeditor"

# Default: list recipes
default:
    @just --list

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
