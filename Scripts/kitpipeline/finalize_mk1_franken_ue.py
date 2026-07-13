r"""
finalize_mk1_franken_ue.py -- UE-side finalize for the Mk1 FRANKENRIFLE body (the picked raw Tripo
candidate mk1_d). Sibling of finalize_raw_bodies_ue.py, but:
  * reads ONE handoff (_rawhandoff_mk1_d.json), not the glob (so it can't touch the Mk6-10 energy bodies),
  * builds a MATTE PBR material (base/normal/rough/metal, NO emissive power-node) -- the Mk1 is cobbled
    junk, not a glowing energy rifle, so it must not self-illuminate like Meshes_Energy,
  * wires LevelBodyMeshes[0]/LevelMuzzleOffsets[0]/LevelHoldScales[0]/LevelFineRot[0] on the parent BP
    and the Mk1 child BP CDO.

Textures MUST be pre-extracted with SYSTEM python 3.12 (glb_extract.py) into mk1_v31\_tex\mk1_d BEFORE
running -- never subprocess sys.executable from UE python (spawns a 2nd editor that deadlocks).

Run headless (game/build closed):
  .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\kitpipeline\finalize_mk1_franken_ue.py
"""
import unreal, os, json

EQ = "/LaserRifleMod/Equipment/LaserRifle"
MESH_DIR = EQ + "/Meshes_Mk1"                 # new home for the frankenrifle body (keeps Rifle_Mk01 template intact)
PARENT_BP = EQ + "/BP_Equip_LaserRifle"
TARGET_CM = 110.0
MK = 1
BID = "mk1_d"
HANDOFF = r"C:\Claude\Projects\SatisfactoryLaserRifleMod\Scripts\kitpipeline\_rawhandoff_mk1_d.json"
TEXDIR = r"C:\Claude\Projects\SatisfactoryLaserRifleMod\tripo_rifles\mk1_v31\_tex\mk1_d"
# Held-orientation correction baked from the user's in-game lr.HoldPitch/HoldYaw pass (2026-07-05).
FINE_ROT = (5.0, -2.0, 0.0)   # (pitch, yaw, roll)

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary

def log(m): unreal.log_warning("LR_MK1FRANKEN | " + str(m))

def fbx_opts(scale):
    o = unreal.FbxImportUI()
    o.import_mesh = True; o.import_as_skeletal = False
    o.import_materials = False; o.import_textures = False
    o.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    sm = o.static_mesh_import_data
    sm.set_editor_property("combine_meshes", True)
    sm.set_editor_property("auto_generate_collision", True)
    sm.set_editor_property("import_uniform_scale", float(scale))
    return o

def import_mesh(fbx, name, scale):
    if eal.does_asset_exist(MESH_DIR + "/" + name):
        eal.delete_asset(MESH_DIR + "/" + name)
    t = unreal.AssetImportTask()
    t.filename = fbx; t.destination_path = MESH_DIR; t.destination_name = name
    t.replace_existing = True; t.automated = True; t.save = True
    t.options = fbx_opts(scale)
    tools.import_asset_tasks([t])
    return MESH_DIR + "/" + name

def longest(path):
    m = eal.load_asset(path)
    if not m: return 0.0
    b = m.get_bounding_box()
    e = b.get_editor_property("max") - b.get_editor_property("min")
    return max(e.x, e.y, e.z)

def import_tex(png, dest, tname, srgb, normal=False):
    if not os.path.exists(png): return None
    t = unreal.AssetImportTask()
    t.filename = png; t.destination_path = dest; t.destination_name = tname
    t.replace_existing = True; t.automated = True; t.save = True
    tools.import_asset_tasks([t])
    tex = eal.load_asset(dest + "/" + tname)
    if tex:
        tex.set_editor_property("srgb", srgb)
        if normal:
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property("flip_green_channel", True)
        eal.save_asset(dest + "/" + tname)
    return tex

def build_matte_material(name, base, normal, rough, metal):
    """Plain lit PBR -- NO emissive. The frankenrifle is scavenged junk; it should read matte,
    not glow like the energy bodies (whose M_Energy raises baseColor to a power into emissive)."""
    mpath = MESH_DIR + "/M_" + name
    if eal.does_asset_exist(mpath): eal.delete_asset(mpath)
    mat = tools.create_asset("M_" + name, MESH_DIR, unreal.Material, unreal.MaterialFactoryNew())
    def samp(tex, y, stype):
        n = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -350, y)
        n.texture = tex; n.set_editor_property("sampler_type", stype); return n
    if base:
        b = samp(base, -150, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        MEL.connect_material_property(b, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    if metal:
        mt = samp(metal, 40, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        MEL.connect_material_property(mt, "R", unreal.MaterialProperty.MP_METALLIC)
    if rough:
        r = samp(rough, 220, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        MEL.connect_material_property(r, "R", unreal.MaterialProperty.MP_ROUGHNESS)
    if normal:
        nm = samp(normal, 400, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        MEL.connect_material_property(nm, "RGB", unreal.MaterialProperty.MP_NORMAL)
    MEL.recompile_material(mat); eal.save_asset(mpath)
    return mat

if not eal.does_directory_exist(MESH_DIR):
    eal.make_directory(MESH_DIR)

data = json.load(open(HANDOFF))
fbx = data["body_only_fbx"]; fy, fz = data["muzzle_frac"]
name = "Body_Mk1_Franken"

# two-pass scale normalize to 110cm length
import_mesh(fbx, name, 1.0)
l1 = longest(MESH_DIR + "/" + name)
sc = round(TARGET_CM / l1, 4) if l1 > 0 else 1.0
import_mesh(fbx, name, sc)
mesh_path = MESH_DIR + "/" + name
mesh = eal.load_asset(mesh_path)
if not mesh:
    log("FAIL import"); raise SystemExit

tdest = MESH_DIR + "/Tex_" + name
if not eal.does_directory_exist(tdest): eal.make_directory(tdest)
base   = import_tex(os.path.join(TEXDIR, "Base Color.png"), tdest, "T_%s_Base" % name, True)
normal = import_tex(os.path.join(TEXDIR, "Normal.png"),     tdest, "T_%s_Normal" % name, False, True)
rough  = import_tex(os.path.join(TEXDIR, "Roughness.png"),  tdest, "T_%s_Rough" % name, False)
metal  = import_tex(os.path.join(TEXDIR, "Metallic.png"),   tdest, "T_%s_Metal" % name, False)
mat = build_matte_material(name, base, normal, rough, metal)

slots = mesh.get_editor_property("static_materials")
ns = []
for i, s in enumerate(slots):
    sm = unreal.StaticMaterial()
    sm.set_editor_property("material_interface", mat if i == 0 else s.get_editor_property("material_interface"))
    sm.set_editor_property("material_slot_name", s.get_editor_property("material_slot_name"))
    ns.append(sm)
mesh.set_editor_property("static_materials", ns)
eal.save_asset(mesh_path)

box = mesh.get_bounding_box()
mn = box.get_editor_property("min"); mx = box.get_editor_property("max")
cy = (mn.y + mx.y)/2.0; hy = (mx.y - mn.y)/2.0
cz = (mn.z + mx.z)/2.0; hz = (mx.z - mn.z)/2.0
muzzle = unreal.Vector(mx.x, cy + fy*hy, cz + fz*hz)   # front-tip emitter (beam origin)
full_z = max(mx.z - mn.z, 1e-3)
hold = round(max(0.20, min(0.70, 33.0 / full_z)), 3)

def resync(bp_path, label):
    bp = eal.load_asset(bp_path)
    if not bp:
        log("BLOCKER: BP not found " + bp_path); return
    cdo = unreal.get_default_object(bp.generated_class())
    cur = list(cdo.get_editor_property("LevelBodyMeshes"))
    while len(cur) < 10: cur.append(None)
    cur[MK-1] = mesh
    cdo.set_editor_property("LevelBodyMeshes", cur)
    muz = list(cdo.get_editor_property("LevelMuzzleOffsets"))
    while len(muz) < 10: muz.append(unreal.Vector(0,0,0))
    muz[MK-1] = muzzle
    cdo.set_editor_property("LevelMuzzleOffsets", muz)
    hs = list(cdo.get_editor_property("LevelHoldScales"))
    while len(hs) < 10: hs.append(0.0)
    hs[MK-1] = hold
    cdo.set_editor_property("LevelHoldScales", hs)
    fr = list(cdo.get_editor_property("LevelFineRot"))
    while len(fr) < 10: fr.append(unreal.Rotator(0.0, 0.0, 0.0))
    fp_, fy_, fr_ = FINE_ROT
    fr[MK-1] = unreal.Rotator(roll=fr_, pitch=fp_, yaw=fy_)
    cdo.set_editor_property("LevelFineRot", fr)
    try: unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    except Exception as e: log("%s compile warn: %s" % (label, e))
    unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
    log("%s: LevelBodyMeshes[%d]=%s muzzle=%s hold=%s saved" % (label, MK-1, name, muzzle, hold))

resync(PARENT_BP, "parent")
resync(EQ + "/BP_Equip_LaserRifle_Mk%d" % MK, "child Mk%d" % MK)
eal.save_directory(MESH_DIR, only_if_is_dirty=False, recursive=True)
log("DONE. Body_Mk1_Franken -> LevelBodyMeshes[0] (matte, base=%s normal=%s rough=%s metal=%s) hold=%s"
    % (bool(base), bool(normal), bool(rough), bool(metal), hold))
