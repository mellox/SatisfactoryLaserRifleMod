r"""
import_rigged_mounts.py -- re-import the rigged Mk1 body WITH the 6 new mount_1..mount_6
anchor bones (added in Blender at the standard mount-fraction points, axes MEASURED from the
rig's own bones -- fwd=-Y, up=+Z in this FBX's local space). Replaces SK_LaserRifle_Mk1c in
place so the existing C++ ctor path keeps working; the skeleton gains the mount bones.

Mirrors import_rigged.py exactly (two-pass scale normalize to 110cm, embedded Tripo PBR
materials, save_directory cook-crash guard).

Run headless (game closed):
  .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\ue\import_rigged_mounts.py
"""
import unreal

FBX      = r"C:\Claude\Projects\SatisfactoryLaserRifleMod\tripo_rifles\rigged\Mk1c_rigged_mounts.fbx"
DEST     = "/LaserRifleMod/Equipment/LaserRifle/Rigged"
NAME     = "SK_LaserRifle_Mk1c"
TARGET_CM = 110.0

tools = unreal.AssetToolsHelpers.get_asset_tools()
eal   = unreal.EditorAssetLibrary

def log(m): unreal.log("LR_ " + m)

def make_task(scale):
    o = unreal.FbxImportUI()
    o.import_mesh = True
    o.import_as_skeletal = True
    o.import_materials = True
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
    b = m.get_bounds()
    ext = b.box_extent
    return 2.0 * max(ext.x, ext.y, ext.z)

log("importing %s with mount bones (pass 1, scale 1.0)" % NAME)
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
    # verify the mount bones actually made it through
    names = [str(n) for n in unreal.SkeletalMeshLibrary.get_bones(sk)] \
        if hasattr(unreal, "SkeletalMeshLibrary") and hasattr(unreal.SkeletalMeshLibrary, "get_bones") else []
    if not names:
        try:
            names = [str(sk.get_editor_property("skeleton").get_editor_property("bone_tree"))]
        except Exception:
            names = ["<bone list API unavailable; check in log via C++ SetupRig diag>"]
    log("bones: %s" % names)
    saved = eal.save_directory(DEST, only_if_is_dirty=False, recursive=True)
    log("save_directory(%s) ok=%s" % (DEST, saved))
log("DONE")
