# populate_kit_component_pool.py -- fill BP_Equip_LaserRifle's (and BP_Equip_LaserRifle_Mk1's)
# CDO KitComponentPool array with the 30 imported kit-component SKELETAL meshes (SK_Kit_*,
# rigged with root + conn_0/conn_1 anchor bones -- the skeletons pass; the old SM_Kit_*
# static pool is superseded).
# Run AFTER a build has compiled the (now TArray<USkeletalMesh>) KitComponentPool property in
# (2-build sequence, same pattern as FixedMkLevel). Run (game/editor/build closed):
#   .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\ue\populate_kit_component_pool.py
import unreal
EQ = "/LaserRifleMod/Equipment/LaserRifle"
KIT_DIR = EQ + "/Components/Kit"
PARENT_BP = EQ + "/BP_Equip_LaserRifle"
MK1_CHILD_BP = EQ + "/BP_Equip_LaserRifle_Mk1"

eal = unreal.EditorAssetLibrary

def log(m): unreal.log_warning("LR_POOLFILL | " + str(m))

names = eal.list_assets(KIT_DIR, recursive=False, include_folder=False)
meshes = []
for path in names:
    obj_path = path.split(".")[0] if "." in path else path
    short = obj_path.rsplit("/", 1)[-1]
    if not short.startswith("SK_Kit_") or short.endswith("_Skeleton"):
        continue
    m = eal.load_asset(obj_path)
    if m and isinstance(m, unreal.SkeletalMesh):
        meshes.append(m)
log("Found %d SK_Kit_* skeletal meshes under %s" % (len(meshes), KIT_DIR))

if len(meshes) < 6:
    log("BLOCKER: fewer than 6 meshes found -- check the import step completed.")
else:
    for bp_path, label in [(PARENT_BP, "parent"), (MK1_CHILD_BP, "child_mk1")]:
        bp = eal.load_asset(bp_path)
        if not bp:
            log("BLOCKER: BP not found " + bp_path)
            continue
        cdo = unreal.get_default_object(bp.generated_class())
        try:
            cdo.set_editor_property("KitComponentPool", meshes)
        except Exception as e:
            log("%s: FAILED to set KitComponentPool -- %s (property may not exist, check build)" % (label, str(e)))
            continue
        try:
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        except Exception as e:
            log("%s compile warn: %s" % (label, str(e)))
        unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
        # read back to confirm
        chk_cdo = unreal.get_default_object(eal.load_asset(bp_path).generated_class())
        chk = chk_cdo.get_editor_property("KitComponentPool")
        log("%s: KitComponentPool set to %d entries, saved, verified read-back=%d" % (label, len(meshes), len(chk)))

log("DONE.")
