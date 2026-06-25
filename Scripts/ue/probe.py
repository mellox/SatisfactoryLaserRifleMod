# probe.py -- de-risk the headless Unreal Python route for LaserRifleMod.
# Creates the 5 content folders, imports ONE FBX (blaster-a) at scale 1.0,
# and reports its bounding box so we can pick the right import scale.
# Run via Scripts/ue/run_ue_python.ps1.
import unreal

KIT = r"C:\Claude\Projects\SatisfactoryLaserRifleMod\kenney_blaster-kit_2.1"
PLUGIN_ROOT = "/LaserRifleMod"
MESH_DIR = PLUGIN_ROOT + "/Equipment/LaserRifle/Meshes"

FOLDERS = [
    PLUGIN_ROOT + "/Equipment/LaserRifle",
    MESH_DIR,
    PLUGIN_ROOT + "/Equipment/LaserRifle/Icons",
    PLUGIN_ROOT + "/Recipes",
    PLUGIN_ROOT + "/Research",
]

def log(msg):
    unreal.log("LR_PROBE | " + str(msg))

log("unreal version: " + unreal.SystemLibrary.get_engine_version())

# --- folders --------------------------------------------------------------
eal = unreal.EditorAssetLibrary
for f in FOLDERS:
    if not eal.does_directory_exist(f):
        ok = eal.make_directory(f)
        log("make_directory %s -> %s" % (f, ok))
    else:
        log("dir exists %s" % f)

# --- import blaster-a at scale 1.0 ---------------------------------------
src = KIT + r"\Models\FBX format\blaster-a.fbx"

opts = unreal.FbxImportUI()
opts.import_mesh = True
opts.import_as_skeletal = False
opts.import_materials = False
opts.import_textures = False
opts.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
sm = opts.static_mesh_import_data
sm.set_editor_property("combine_meshes", True)
sm.set_editor_property("auto_generate_collision", True)
sm.set_editor_property("import_uniform_scale", 1.0)

task = unreal.AssetImportTask()
task.filename = src
task.destination_path = MESH_DIR
task.destination_name = "blaster-a"
task.replace_existing = True
task.automated = True
task.save = True
task.options = opts

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
log("imported paths: %s" % list(task.imported_object_paths))

# --- report bounding box --------------------------------------------------
asset_path = MESH_DIR + "/blaster-a"
if eal.does_asset_exist(asset_path):
    mesh = eal.load_asset(asset_path)
    try:
        bmin, bmax = mesh.get_bounding_box()
        ext = (bmax - bmin)
        log("BBOX min=%s max=%s extent=(%.2f, %.2f, %.2f)" % (bmin, bmax, ext.x, ext.y, ext.z))
        longest = max(ext.x, ext.y, ext.z)
        log("LONGEST_AXIS_CM=%.3f  (suggest import_uniform_scale ~ %.0f for a ~100cm rifle)" %
            (longest, (100.0 / longest) if longest > 0 else 0))
    except Exception as e:
        log("bbox failed: %s" % e)
    log("PROBE_RESULT=PASS")
else:
    log("PROBE_RESULT=FAIL asset not created: %s" % asset_path)
