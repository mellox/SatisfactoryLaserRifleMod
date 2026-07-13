# verify_kit_probe.py -- READ-ONLY check: does the SAVED BP_Equip_LaserRifle /
# BP_Equip_LaserRifle_Mk1 CDO actually reference the kit-test mesh right now?
# No edits, no build. Run (game/editor/build closed):
#   .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\ue\verify_kit_probe.py
import unreal
EQ = "/LaserRifleMod/Equipment/LaserRifle"
eal = unreal.EditorAssetLibrary

def log(m): unreal.log_warning("LR_VERIFY | " + str(m))

for path, label in [(EQ + "/BP_Equip_LaserRifle", "parent"),
                     (EQ + "/BP_Equip_LaserRifle_Mk1", "child_mk1")]:
    bp = eal.load_asset(path)
    if not bp:
        log("%s: BP NOT FOUND at %s" % (label, path))
        continue
    cdo = unreal.get_default_object(bp.generated_class())
    meshes = cdo.get_editor_property("LevelBodyMeshes")
    m0 = meshes[0] if meshes else None
    fixed_mk = None
    try:
        fixed_mk = cdo.get_editor_property("FixedMkLevel")
    except Exception:
        pass
    log("%s: FixedMkLevel=%s LevelBodyMeshes[0]=%s" % (
        label, fixed_mk, m0.get_name() if m0 else "NONE"))

mesh = eal.load_asset(EQ + "/Meshes_KitTest/KitTest_Mk1Junk")
log("KitTest_Mk1Junk mesh asset exists: %s" % bool(mesh))
