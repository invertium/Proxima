"""Provenance: how the Fab Earth (earth.blend) became the in-game planet backdrop.

Two stages. STAGE A runs in headless Blender (`python3 tools/_import_earth.py --blend`),
STAGE B runs inside the editor via MCP (`python3 tools/vibe_py.py tools/_import_earth.py`).

Key gotchas learned:
  * UE can't import .blend — export geometry first.
  * Exporting FBX imported as a *SkeletalMesh* no matter the FbxImportUI flags (the Fab
    mesh sits under empties; UE auto-classifies it skeletal). **OBJ** has no skeleton
    concept and always imports static — use OBJ.
  * Asset imports over the live MCP path crash via Interchange — disable the Interchange
    importers first so the legacy synchronous factory is used. See
    memory: fbx-import-interchange-crash.
"""
import sys

BLEND = "/home/julian/gitrepos/spaceGame/_assets_dl/earth.blend"
EXPORT = "/home/julian/gitrepos/spaceGame/_assets_dl/earth_export"


def stage_a_blender():
    import bpy, os
    os.makedirs(EXPORT, exist_ok=True)
    bpy.ops.wm.open_mainfile(filepath=BLEND)
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    # Save the packed 8K equirectangular texture for a hand-built material.
    for img in bpy.data.images:
        if img.size[0] > 0:
            img.filepath_raw = os.path.join(EXPORT, "Image_0.png")
            img.file_format = "PNG"
            img.save()
            break
    bpy.ops.wm.obj_export(filepath=os.path.join(EXPORT, "earth.obj"),
                          export_selected_objects=True, export_uv=True, export_normals=True,
                          export_materials=False, forward_axis="NEGATIVE_Z", up_axis="Y")
    print("STAGE A done ->", EXPORT)


def stage_b_unreal():
    import unreal
    for c in ("Interchange.FeatureFlags.Import.OBJ 0", "Interchange.FeatureFlags.Import.PNG 0"):
        unreal.SystemLibrary.execute_console_command(None, c)
    eal = unreal.EditorAssetLibrary
    at = unreal.AssetToolsHelpers.get_asset_tools()
    ME = unreal.MaterialEditingLibrary

    # OBJ mesh -> static.
    t = unreal.AssetImportTask()
    t.filename = EXPORT + "/earth.obj"
    t.destination_path = "/Game/Art/Meshes"
    t.destination_name = "Earth"
    t.automated = t.replace_existing = t.save = True
    at.import_asset_tasks([t])

    # 8K texture.
    tt = unreal.AssetImportTask()
    tt.filename = EXPORT + "/Image_0.png"
    tt.destination_path = "/Game/Art/Textures"
    tt.destination_name = "T_Earth"
    tt.automated = tt.replace_existing = tt.save = True
    at.import_asset_tasks([tt])

    # Lit Earth material: albedo + faint self-fill so the night side isn't pure black.
    tex = eal.load_asset("/Game/Art/Textures/T_Earth")
    mat = at.create_asset("M_Earth", "/Game/Art/Materials", unreal.Material, unreal.MaterialFactoryNew())
    ts = ME.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -500, 0)
    ts.set_editor_property("texture", tex)
    ME.connect_material_property(ts, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = ME.create_material_expression(mat, unreal.MaterialExpressionConstant, -500, 250)
    rough.set_editor_property("r", 0.8)
    ME.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    fill = ME.create_material_expression(mat, unreal.MaterialExpressionConstant, -500, 350)
    fill.set_editor_property("r", 0.18)
    emul = ME.create_material_expression(mat, unreal.MaterialExpressionMultiply, -250, 200)
    ME.connect_material_expressions(ts, "RGB", emul, "A")
    ME.connect_material_expressions(fill, "", emul, "B")
    ME.connect_material_property(emul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    ME.recompile_material(mat)
    eal.save_asset("/Game/Art/Materials/M_Earth", False)
    m = eal.load_asset("/Game/Art/Meshes/Earth")
    m.set_material(0, mat)
    eal.save_asset("/Game/Art/Meshes/Earth", False)

    # Place as a ~9360uu planet (mesh radius ~36uu * 260) off to one side.
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    act = eas.spawn_actor_from_class(unreal.StaticMeshActor,
                                     unreal.Vector(13000, 12000, -1500), unreal.Rotator(0, -35, 0))
    act.set_actor_label("Earth_Backdrop")
    act.set_actor_scale3d(unreal.Vector(260, 260, 260))
    smc = act.static_mesh_component
    smc.set_static_mesh(m)
    smc.set_material(0, mat)
    smc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    smc.set_cast_shadow(False)
    act.set_actor_enable_collision(False)
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    print("STAGE B done -> Earth_Backdrop placed + level saved")


if "--blend" in sys.argv:
    stage_a_blender()
else:
    stage_b_unreal()
