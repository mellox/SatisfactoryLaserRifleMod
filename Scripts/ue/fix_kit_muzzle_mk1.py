# fix_kit_muzzle_mk1.py -- fix pass 4: recompute LevelMuzzleOffsets[0] using a front-slice-
# centroid FRACTION (fy, fz) instead of whole-bbox-center. Same formula as import_tripo.py's
# proven per-Mk muzzle correction: X = bbox Max.X, Y/Z = bbox center + frac*half_extent.
# No mesh re-import needed -- Rifle_Mk01's geometry is unchanged, only the muzzle offset
# calculation was wrong. Run (game/editor/build closed):
#   .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\ue\fix_kit_muzzle_mk1.py
import unreal
EQ = "/LaserRifleMod/Equipment/LaserRifle"
MESH_PATH = EQ + "/Meshes_Tripo/Rifle_Mk01"
PARENT_BP = EQ + "/BP_Equip_LaserRifle"
MK1_CHILD_BP = EQ + "/BP_Equip_LaserRifle_Mk1"
FY, FZ = 0.025738770452917184, 0.6108623665907187   # from finalize_kit_rifle_blender.py, mk1

eal = unreal.EditorAssetLibrary

def log(m): unreal.log_warning("LR_MUZZLEFIX | " + str(m))

mesh = eal.load_asset(MESH_PATH)
if not mesh:
    log("BLOCKER: mesh not found " + MESH_PATH)
else:
    box = mesh.get_bounding_box()
    mn = box.get_editor_property("min"); mx = box.get_editor_property("max")
    cy = (mn.y + mx.y) / 2.0; hy = (mx.y - mn.y) / 2.0
    cz = (mn.z + mx.z) / 2.0; hz = (mx.z - mn.z) / 2.0
    muzzle = unreal.Vector(mx.x, cy + FY * hy, cz + FZ * hz)
    log("bbox=(%.1f..%.1f, %.1f..%.1f, %.1f..%.1f) -> muzzle=(%.1f,%.1f,%.1f)" % (
        mn.x, mx.x, mn.y, mx.y, mn.z, mx.z, muzzle.x, muzzle.y, muzzle.z))

    for bp_path, label in [(PARENT_BP, "parent"), (MK1_CHILD_BP, "child_mk1")]:
        bp = eal.load_asset(bp_path)
        if not bp:
            log("BLOCKER: BP not found " + bp_path); continue
        cdo = unreal.get_default_object(bp.generated_class())
        muz = list(cdo.get_editor_property("LevelMuzzleOffsets"))
        if len(muz) != 10:
            log("%s: LevelMuzzleOffsets length %d != 10, skipped" % (label, len(muz))); continue
        prev = muz[0]
        muz[0] = muzzle
        cdo.set_editor_property("LevelMuzzleOffsets", muz)
        try:
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        except Exception as e:
            log("%s compile warn: %s" % (label, str(e)))
        unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
        chk_cdo = unreal.get_default_object(eal.load_asset(bp_path).generated_class())
        chk = chk_cdo.get_editor_property("LevelMuzzleOffsets")[0]
        log("%s: muzzle %s -> %s, saved, verified read-back=%s" % (label, prev, muzzle, chk))

    log("DONE. Rebuild to cook (bump C++ marker first).")
