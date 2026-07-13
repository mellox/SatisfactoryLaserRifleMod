# zero_muzzle_offsets.py -- clear BP_Equip_LaserRifle's LevelMuzzleOffsets so the runtime
# auto-derives each Mk's muzzle from the mesh's front (+X) bbox tip. All 10 rifle meshes were
# re-worked (2026-07-03..07-05) AFTER the muzzle points were baked (2026-06-25), so the old
# per-Mk offsets point behind the new barrels. Empty array => FireLaser's IsValidIndex check
# fails => it uses the mesh-bbox auto-guess (LaserRifleWeapon.cpp ~1937). Leaves LevelFineRot
# and LevelHoldScales untouched.
# Run (game + editor CLOSED): .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\ue\zero_muzzle_offsets.py
import unreal, sys
P = "LR_ZEROMUZ"
def log(m):
    print(P + " " + str(m))
    try: unreal.log_warning(P + " " + str(m))
    except Exception: pass
eal = unreal.EditorAssetLibrary
ASSET = "/LaserRifleMod/Equipment/LaserRifle/BP_Equip_LaserRifle"
def n(cdo, prop):
    v = cdo.get_editor_property(prop)
    return len(v) if v is not None else 0
def main():
    bp = eal.load_asset(ASSET)
    if not bp:
        log("MISSING " + ASSET); return False
    cdo = unreal.get_default_object(bp.generated_class())
    log("BEFORE: Muzzle=%d FineRot=%d HoldScales=%d KitPool=%d" % (
        n(cdo, "LevelMuzzleOffsets"), n(cdo, "LevelFineRot"), n(cdo, "LevelHoldScales"), n(cdo, "KitComponentPool")))
    cdo.set_editor_property("LevelMuzzleOffsets", [])   # empty => runtime auto-derives from mesh tip
    try:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    except Exception as e:
        log("compile note: %s" % e)
    log("save -> %s" % eal.save_loaded_asset(bp))
    cdo2 = unreal.get_default_object(eal.load_asset(ASSET).generated_class())
    mz, fr, hs = n(cdo2, "LevelMuzzleOffsets"), n(cdo2, "LevelFineRot"), n(cdo2, "LevelHoldScales")
    log("AFTER:  Muzzle=%d (want 0)  FineRot=%d (want 10)  HoldScales=%d (want 10)" % (mz, fr, hs))
    ok = (mz == 0 and fr == 10 and hs == 10)
    log("RESULT: %s" % ("OK -- muzzle cleared, other tuning preserved" if ok else "PROBLEM -- check counts"))
    return ok
try:
    ok = main(); log("Script " + ("SUCCEEDED" if ok else "FAILED")); sys.exit(0 if ok else 1)
except Exception as e:
    import traceback; log("Unhandled " + str(e)); traceback.print_exc(); sys.exit(1)
