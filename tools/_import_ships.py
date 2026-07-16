"""One-off: import Quaternius Insurgent (player) + Imperial (enemy) FBX + textures,
build a lit material per ship from its color-variant palette texture, assign to mesh."""
import unreal
import os

# Interchange's async FBX import re-enters the MCP task-graph thread and asserts.
# Fall back to the legacy synchronous FBX/texture factories before importing.
for _cmd in (
    "Interchange.FeatureFlags.Import.FBX 0",
    "Interchange.FeatureFlags.Import.PNG 0",
    "Interchange.FeatureFlags.Import.GLTF 0",
):
    try:
        unreal.SystemLibrary.execute_console_command(None, _cmd)
    except Exception as _e:
        print("cvar skip:", _cmd, _e)

_REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
BASE = os.path.join(_REPO, "_assets_dl", "Ultimate Spaceships - May 2021")

# (ship folder, color variant, content name)
SHIPS = [
    ("Insurgent", "Blue", "Insurgent"),   # player fighter
    ("Imperial",  "Red",  "Imperial"),    # enemy cruiser
]

MESH_DIR = "/Game/Art/Meshes"
TEX_DIR  = "/Game/Art/Textures"
MAT_DIR  = "/Game/Art/Materials"

at = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def import_one(src, dest_dir, factory=None, fbx=False):
    task = unreal.AssetImportTask()
    task.filename = src
    task.destination_path = dest_dir
    task.automated = True
    task.replace_existing = True
    task.save = True
    if fbx:
        ui = unreal.FbxImportUI()
        ui.import_mesh = True
        ui.import_as_skeletal = False
        ui.import_materials = False
        ui.import_textures = False
        ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
        sm = ui.static_mesh_import_data
        sm.set_editor_property("combine_meshes", True)
        sm.set_editor_property("import_uniform_scale", 1.0)
        task.options = ui
    at.import_asset_tasks([task])
    return task.imported_object_paths


for folder, color, name in SHIPS:
    fbx_src = os.path.join(BASE, folder, "FBX", folder + ".fbx")
    tex_src = os.path.join(BASE, folder, "Textures", "%s_%s.png" % (folder, color))

    print("=== %s ===" % name)
    print(" mesh:", import_one(fbx_src, MESH_DIR, fbx=True))
    print(" tex :", import_one(tex_src, TEX_DIR))

    tex_path = "%s/%s_%s" % (TEX_DIR, folder, color)
    tex = eal.load_asset(tex_path)
    if not tex:
        # texture may import under the source filename without color? try folder name
        tex_path = "%s/%s" % (TEX_DIR, "%s_%s" % (folder, color))
        tex = eal.load_asset(tex_path)
    print(" loaded tex:", tex_path, bool(tex))

    # Build a simple lit material: TextureSample -> BaseColor, constant roughness.
    mfac = unreal.MaterialFactoryNew()
    mat = at.create_asset("M_%s" % name, MAT_DIR, unreal.Material, mfac)
    mel = unreal.MaterialEditingLibrary
    ts = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -380, 0)
    if tex:
        ts.set_editor_property("texture", tex)
    mel.connect_material_property(ts, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -380, 220)
    rough.set_editor_property("r", 0.55)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    metal = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -380, 320)
    metal.set_editor_property("r", 0.1)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(mat)
    eal.save_asset("%s/M_%s" % (MAT_DIR, name), False)

    # Assign material to the imported static mesh slot 0.
    mesh = eal.load_asset("%s/%s" % (MESH_DIR, folder))
    if mesh:
        mesh.set_material(0, mat)
        eal.save_asset("%s/%s" % (MESH_DIR, folder), False)
        b = mesh.get_bounds().box_extent
        print(" mesh bounds extent:", b.x, b.y, b.z)
    print(" done %s" % name)

print("ALL DONE")
