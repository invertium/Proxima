"""Add a space nebula backdrop:
   1) flip M_Starfield to additive so its black background reads as transparent (stars kept);
   2) author M_Nebula (unlit, two-sided) — dark base + soft directional colour blobs;
   3) place a Nebula_Dome sphere behind the starfield dome; save."""
import unreal

ME = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# --- 1) Starfield -> additive (black becomes transparent; stars add over the nebula) ---
star = eal.load_asset("/Game/Materials/M_Starfield")
star.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
ME.recompile_material(star)
eal.save_asset("/Game/Materials/M_Starfield", False)
print("M_Starfield -> additive")

# --- 2) M_Nebula ---
if eal.does_asset_exist("/Game/Materials/M_Nebula"):
    eal.delete_asset("/Game/Materials/M_Nebula")
mat = at.create_asset("M_Nebula", "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("two_sided", True)

def c3(x, y, z, px, py):
    e = ME.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, px, py)
    e.set_editor_property("constant", unreal.LinearColor(x, y, z, 1.0))
    return e

# View direction on the dome surface.
wp = ME.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1400, 0)
ndir = ME.create_material_expression(mat, unreal.MaterialExpressionNormalize, -1150, 0)
ME.connect_material_expressions(wp, "", ndir, "")

# Dark deep-space base.
base = c3(0.006, 0.007, 0.016, -900, -260)

def blob(dirx, diry, dirz, r, g, b, power, px):
    d = c3(dirx, diry, dirz, px, 180)
    dot = ME.create_material_expression(mat, unreal.MaterialExpressionDotProduct, px + 230, 60)
    ME.connect_material_expressions(ndir, "", dot, "A")
    ME.connect_material_expressions(d, "", dot, "B")
    sat = ME.create_material_expression(mat, unreal.MaterialExpressionSaturate, px + 430, 60)
    ME.connect_material_expressions(dot, "", sat, "")
    pw = ME.create_material_expression(mat, unreal.MaterialExpressionPower, px + 600, 60)
    pw.set_editor_property("const_exponent", float(power))
    ME.connect_material_expressions(sat, "", pw, "Base")
    col = c3(r, g, b, px + 600, 240)
    mul = ME.create_material_expression(mat, unreal.MaterialExpressionMultiply, px + 800, 120)
    ME.connect_material_expressions(pw, "", mul, "A")
    ME.connect_material_expressions(col, "", mul, "B")
    return mul

# Three overlapping colour clouds (magenta, teal, dim blue).
b1 = blob(0.74, 0.25, 0.62, 0.060, 0.012, 0.075, 6.0, -300)
b2 = blob(-0.56, 0.70, -0.42, 0.010, 0.045, 0.055, 8.0, 700)
b3 = blob(0.20, -0.30, 0.93, 0.018, 0.020, 0.060, 5.0, 1700)

add1 = ME.create_material_expression(mat, unreal.MaterialExpressionAdd, 900, -120)
ME.connect_material_expressions(base, "", add1, "A")
ME.connect_material_expressions(b1, "", add1, "B")
add2 = ME.create_material_expression(mat, unreal.MaterialExpressionAdd, 1080, -60)
ME.connect_material_expressions(add1, "", add2, "A")
ME.connect_material_expressions(b2, "", add2, "B")
add3 = ME.create_material_expression(mat, unreal.MaterialExpressionAdd, 1260, 0)
ME.connect_material_expressions(add2, "", add3, "A")
ME.connect_material_expressions(b3, "", add3, "B")
ME.connect_material_property(add3, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
ME.recompile_material(mat)
eal.save_asset("/Game/Materials/M_Nebula", False)
print("M_Nebula authored")

# --- 3) Nebula dome behind the starfield (radius ~30000 vs starfield ~20000) ---
for a in eas.get_all_level_actors():
    if a.get_actor_label() == "Nebula_Dome":
        eas.destroy_actor(a)
sphere = eal.load_asset("/Engine/BasicShapes/Sphere.Sphere")
dome = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
dome.set_actor_label("Nebula_Dome")
dome.set_actor_scale3d(unreal.Vector(600, 600, 600))
smc = dome.static_mesh_component
smc.set_static_mesh(sphere)
smc.set_material(0, mat)
smc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
smc.set_cast_shadow(False)
print("Nebula_Dome placed")

unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
print("level saved. DONE")
