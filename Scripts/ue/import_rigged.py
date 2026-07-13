r"""
import_rigged.py -- import the rigged, decimated Mk1 (candidate c) as a SKELETAL MESH
into the LaserRifleMod plugin, so the C++ procedural bone driver can pose its
antenna + dangling-battery/wire bones at runtime.

Mirrors import_tripo.py conventions (barrel +X, normalize to TARGET_CM long) but:
  - imports as SKELETAL (creates SK_ + its Skeleton + PhysicsAsset)
  - uses Tripo's EMBEDDED PBR materials/textures (pilot; surface is swappable later)

Run headless (game closed):  .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\ue\import_rigged.py
"""
import unreal, os

FBX      = r"C:\Claude\Projects\SatisfactoryLaserRifleMod\tripo_rifles\rigged\Mk1c_rigged.fbx"
DEST     = "/LaserRifleMod/Equipment/LaserRifle/Rigged"
NAME     = "SK_LaserRifle_Mk1c"
TARGET_CM = 110.0   # match import_tripo.py so the rigged rifle is the same in-hand size

tools = unreal.AssetToolsHelpers.get_asset_tools()
eal   = unreal.EditorAssetLibrary

def log(m): unreal.log("LR_ " + m)

def make_task(scale):
    o = unreal.FbxImportUI()
    o.import_mesh = True
    o.import_as_skeletal = True
    o.import_materials = True      # use Tripo embedded PBR (pilot)
    o.import_textures  = True
    o.import_animations = False
    o.create_physics_asset = True
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
    b = m.get_bounds()                     # FBoxSphereBounds
    ext = b.box_extent                     # half-extents
    return 2.0 * max(ext.x, ext.y, ext.z)

# --- pass 1: import at scale 1.0, measure, then re-import at the scale that hits TARGET_CM ---
log("importing %s (pass 1, scale 1.0)" % NAME)
tools.import_asset_tasks([make_task(1.0)])
asset_path = DEST + "/" + NAME
l1 = longest_cm(asset_path)
log("pass1 longest = %.3f cm" % l1)
scale = round(TARGET_CM / l1, 4) if l1 > 0 else 1.0
log("re-import at scale = %s -> target %.0f cm" % (scale, TARGET_CM))
tools.import_asset_tasks([make_task(scale)])

# --- verify ---
sk = eal.load_asset(asset_path)
if not sk:
    log("FAIL: skeletal mesh did not import")
else:
    l2 = longest_cm(asset_path)
    skel = sk.get_editor_property("skeleton")
    log("imported SK=%s skeleton=%s longest=%.2fcm" %
        (sk.get_name(), skel.get_name() if skel else "<none>", l2))
    # confirm the rig bones survived export (root + antenna_0/1 + pend_0/1/2)
    try:
        for asset in eal.list_assets(DEST, recursive=False):
            log("  asset: " + asset)
    except Exception as e:
        log("  list err %s" % e)
    # Save the WHOLE directory, not just the SK mesh: the import auto-creates the _Skeleton,
    # _PhysicsAsset, and per-part materials/textures, and t.save only persisted the primary asset.
    # If the _Skeleton .uasset isn't on disk, the cook drops it -> runtime "missing skeleton" crash.
    saved = eal.save_directory(DEST, only_if_is_dirty=False, recursive=True)
    log("save_directory(%s) ok=%s" % (DEST, saved))
log("DONE")
