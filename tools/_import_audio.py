"""Import the 4 SFX WAVs as SoundWave assets into /Game/Audio."""
import os
import unreal

# Be safe re: the Interchange async-import crash (audio likely legacy, but disable anyway).
for c in ("Interchange.FeatureFlags.Import.FBX 0",):
    unreal.SystemLibrary.execute_console_command(None, c)

at = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
_REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
BASE = os.path.join(_REPO, "_assets_dl", "audio", "wav")

for name in ("S_BeamFire", "S_EnemyFire", "S_Hit", "S_Explosion"):
    t = unreal.AssetImportTask()
    t.filename = "%s/%s.wav" % (BASE, name)
    t.destination_path = "/Game/Audio"
    t.destination_name = name
    t.automated = t.replace_existing = t.save = True
    at.import_asset_tasks([t])
    a = eal.load_asset("/Game/Audio/%s" % name)
    print(name, "->", a.get_class().get_name() if a else "FAILED")
print("DONE")
