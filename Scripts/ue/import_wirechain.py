r"""
import_wirechain.py -- import SK_WireChain (thin tube + 8-bone chain authored in Blender)
as a SKELETAL MESH. This is the runtime-posable connector wire for the random kit loadout:
C++ places one UPoseableMeshComponent per connector and poses wire_0..wire_7 along the
live sag curve every frame (real bones, per the user's explicit direction).

Mirrors import_rigged.py (the proven skeletal-import path, incl. the save_directory
cook-crash guard). Run headless (game closed):
  .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\ue\import_wirechain.py
"""
import unreal

FBX       = r"C:\Claude\Projects\SatisfactoryLaserRifleMod\tripo_rifles\rigged\SK_WireChain.fbx"
DEST      = "/LaserRifleMod/Equipment/LaserRifle/Rigged"
NAME      = "SK_WireChain"
TARGET_CM = 100.0   # bone spacing = 12.5cm; runtime pose scales to each connector's real span

tools = unreal.AssetToolsHelpers.get_asset_tools()
eal   = unreal.EditorAssetLibrary

def log(m): unreal.log("LR_ " + m)

def make_task(scale):
    o = unreal.FbxImportUI()
    o.import_mesh = True
    o.import_as_skeletal = True
    o.import_materials = False   # gray default; C++ overrides with the body material
    o.import_textures  = False
    o.import_animations = False
    o.create_physics_asset = False   # purely kinematic, posed from C++
    o.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    sk = o.skeletal_mesh_import_data
    sk.set_editor_property("import_morph_targets", False)
    sk.set_editor_property("use_t0_as_ref_pose", True)
    sk.set_editor_property("import_uniform_scale", float(scale))
    sk.set_editor_property("convert_scene", True)
    sk.set_editor_property("normal_import_method",
                           unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)
    t = unreal.AssetImportTask()
    t.filename = FBX
    t.destination_path = DEST
    t.destination_name = NAME
    t.automated = True
    t.replace_existing = True
    t.save = True
    t.options = o
    return t

def longest_cm(asset_path):
    m = eal.load_asset(asset_path)
    if not m:
        return 0.0
    b = m.get_bounds()
    ext = b.box_extent
    return 2.0 * max(ext.x, ext.y, ext.z)

log("importing %s (pass 1, scale 1.0)" % NAME)
tools.import_asset_tasks([make_task(1.0)])
asset_path = DEST + "/" + NAME
l1 = longest_cm(asset_path)
log("pass1 longest = %.3f cm" % l1)
scale = round(TARGET_CM / l1, 4) if l1 > 0 else 1.0
log("re-import at scale = %s -> target %.0f cm" % (scale, TARGET_CM))
tools.import_asset_tasks([make_task(scale)])

sk = eal.load_asset(asset_path)
if not sk:
    log("FAIL: skeletal mesh did not import")
else:
    l2 = longest_cm(asset_path)
    skel = sk.get_editor_property("skeleton")
    log("imported SK=%s skeleton=%s longest=%.2fcm" %
        (sk.get_name(), skel.get_name() if skel else "<none>", l2))
    for asset in eal.list_assets(DEST, recursive=False):
        log("  asset: " + asset)
    # save the WHOLE directory: import auto-creates the _Skeleton, and t.save only persists
    # the primary asset -- an unsaved skeleton cooks away and crashes on equip (see memory:
    # sf-skeletal-mesh-import-crash).
    saved = eal.save_directory(DEST, only_if_is_dirty=False, recursive=True)
    log("save_directory(%s) ok=%s" % (DEST, saved))
log("DONE")
