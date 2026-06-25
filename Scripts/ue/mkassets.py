# mkassets.py -- LaserRifleMod checklist steps 3-6 (folders, meshes, texture, material, icons).
# Headless; run via Scripts/ue/run_ue_python.ps1. Idempotent (replace_existing).
import unreal

KIT      = r"C:\Claude\Projects\SatisfactoryLaserRifleMod\kenney_blaster-kit_2.1"
FBX_DIR  = KIT + r"\Models\FBX format"
TEX_FILE = KIT + r"\Models\Textures\variation-a.png"   # guide's variation-a (not in FBX/Textures)
PREV_DIR = KIT + r"\Previews"

ROOT     = "/LaserRifleMod"
EQ       = ROOT + "/Equipment/LaserRifle"
MESH_DIR = EQ + "/Meshes"
ICON_DIR = EQ + "/Icons"

# Mk1..Mk10 -> Kenney letter (checklist step 4 order).
ORDER = ["a", "i", "b", "h", "e", "c", "d", "k", "l", "r"]
TARGET_CM = 110.0   # desired longest-axis length of the rifle body

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

def log(m): unreal.log("LR_MK | " + str(m))
def warn(m): unreal.log_warning("LR_MK | " + str(m))

# --- folders --------------------------------------------------------------
for d in [EQ, MESH_DIR, ICON_DIR, ROOT + "/Recipes", ROOT + "/Research"]:
    if not eal.does_directory_exist(d):
        eal.make_directory(d)
log("folders ready")

# --- fbx import option builder -------------------------------------------
def fbx_opts(scale):
    o = unreal.FbxImportUI()
    o.import_mesh = True
    o.import_as_skeletal = False
    o.import_materials = False
    o.import_textures = False
    o.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    sm = o.static_mesh_import_data
    sm.set_editor_property("combine_meshes", True)
    sm.set_editor_property("auto_generate_collision", True)
    sm.set_editor_property("import_uniform_scale", float(scale))
    return o

def import_fbx(letter, scale, dest_name):
    t = unreal.AssetImportTask()
    t.filename = FBX_DIR + ("\\blaster-%s.fbx" % letter)
    t.destination_path = MESH_DIR
    t.destination_name = dest_name
    t.replace_existing = True
    t.automated = True
    t.save = True
    t.options = fbx_opts(scale)
    tools.import_asset_tasks([t])
    return list(t.imported_object_paths)

def longest_axis_cm(asset_path):
    mesh = eal.load_asset(asset_path)
    box = mesh.get_bounding_box()
    bmin = box.get_editor_property("min")
    bmax = box.get_editor_property("max")
    ext = bmax - bmin
    return max(ext.x, ext.y, ext.z)

# --- calibrate scale from blaster-a at scale 1.0 -------------------------
import_fbx("a", 1.0, "blaster-a")
l1 = longest_axis_cm(MESH_DIR + "/blaster-a")
SCALE = round(TARGET_CM / l1) if l1 > 0 else 100.0
if SCALE < 1: SCALE = 1.0
log("calibrate: longest@scale1=%.3fcm -> import_uniform_scale=%s (target %.0fcm)" % (l1, SCALE, TARGET_CM))

# --- import all 10 in Mk order at calibrated scale -----------------------
for i, letter in enumerate(ORDER, start=1):
    paths = import_fbx(letter, SCALE, "blaster-%s" % letter)
    ok = len(paths) > 0 and eal.does_asset_exist(MESH_DIR + "/blaster-%s" % letter)
    log("Mk%02d  blaster-%s  -> %s" % (i, letter, "OK" if ok else "FAIL"))

# --- texture --------------------------------------------------------------
def import_texture(src, dest_dir, name):
    t = unreal.AssetImportTask()
    t.filename = src
    t.destination_path = dest_dir
    t.destination_name = name
    t.replace_existing = True
    t.automated = True
    t.save = True
    tools.import_asset_tasks([t])
    return eal.does_asset_exist(dest_dir + "/" + name)

TEX_ASSET = EQ + "/T_LaserRifle_Body"
tex_ok = import_texture(TEX_FILE, EQ, "T_LaserRifle_Body")
log("texture T_LaserRifle_Body -> %s" % ("OK" if tex_ok else "FAIL"))

# --- material M_LaserRifle_Body (Tint vector + Emissive color/intensity) --
MEL = unreal.MaterialEditingLibrary
MAT_PATH = EQ + "/M_LaserRifle_Body"
if eal.does_asset_exist(MAT_PATH):
    eal.delete_asset(MAT_PATH)
mat = tools.create_asset("M_LaserRifle_Body", EQ, unreal.Material, unreal.MaterialFactoryNew())

# base color = TextureSample.RGB * Tint
ts = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -800, -100)
if tex_ok:
    ts.texture = eal.load_asset(TEX_ASSET)
tint = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -800, 150)
tint.set_editor_property("parameter_name", "Tint")
tint.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
mul_base = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -450, 0)
MEL.connect_material_expressions(ts, "RGB", mul_base, "A")
MEL.connect_material_expressions(tint, "", mul_base, "B")
MEL.connect_material_property(mul_base, "", unreal.MaterialProperty.MP_BASE_COLOR)

# emissive = EmissiveColor * EmissiveIntensity  (Mk1 emerald default)
ecol = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -800, 400)
ecol.set_editor_property("parameter_name", "EmissiveColor")
ecol.set_editor_property("default_value", unreal.LinearColor(0.18, 0.80, 0.44, 1.0))  # #2ECC71
eint = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -800, 560)
eint.set_editor_property("parameter_name", "EmissiveIntensity")
eint.set_editor_property("default_value", 2.0)
mul_emis = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -450, 450)
MEL.connect_material_expressions(ecol, "", mul_emis, "A")
MEL.connect_material_expressions(eint, "", mul_emis, "B")
MEL.connect_material_property(mul_emis, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

MEL.recompile_material(mat)
eal.save_asset(MAT_PATH)
log("material M_LaserRifle_Body -> OK (params: Tint, EmissiveColor, EmissiveIntensity)")

# --- icons (optional, step 6): Previews/blaster-<letter>.png -> Mk icons --
icon_ok = 0
for i, letter in enumerate(ORDER, start=1):
    src = PREV_DIR + ("\\blaster-%s.png" % letter)
    name = "T_LaserRifle_Icon_Mk%d" % i
    if import_texture(src, ICON_DIR, name):
        icon_ok += 1
log("icons imported: %d/10" % icon_ok)

log("MK_RESULT=DONE")
