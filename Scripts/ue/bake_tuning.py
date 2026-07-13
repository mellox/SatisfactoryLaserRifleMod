# bake_tuning.py -- bake the muzzle offsets + Mk7/Mk8 hold (FineRot) the user dialed in live
# (2026-07-13 session, values read from FactoryGame.log BEAMDIAG + lr.HoldPitch echoes) into
# BP_Equip_LaserRifle. Muzzle = final BEAMDIAG muzzleLocal per Mk; FineRot updates only Mk7 (2->4)
# and Mk8 (9->8), rest preserved. LevelHoldScales untouched.
# Run (game + editor CLOSED): .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\ue\bake_tuning.py
import unreal, sys
P = "LR_BAKE"
def log(m):
    print(P + " " + str(m))
    try: unreal.log_warning(P + " " + str(m))
    except Exception: pass
eal = unreal.EditorAssetLibrary
ASSET = "/LaserRifleMod/Equipment/LaserRifle/BP_Equip_LaserRifle"
# index 0 = Mk1 .. 9 = Mk10
MUZ  = [(59,-13,8),(57.4,1.9,19.6),(53,-3,6),(51,-1,7),(46,-6,12),(55,-5,17),(48,0,14),(48,-3,5),(48,-3,20),(52,2,5)]
FINE = [(5,-2,0),(-1,-5,0),(4,0,0),(6,0,0),(3,0,0),(3,0,0),(4,0,0),(8,0,0),(1,-6,0),(5,0,0)]  # (pitch,yaw,roll); Mk7->4, Mk8->8
def main():
    bp = eal.load_asset(ASSET)
    if not bp:
        log("MISSING " + ASSET); return False
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("LevelMuzzleOffsets", [unreal.Vector(x, y, z) for (x, y, z) in MUZ])
    cdo.set_editor_property("LevelFineRot", [unreal.Rotator(pitch=p, yaw=yw, roll=rl) for (p, yw, rl) in FINE])
    try:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    except Exception as e:
        log("compile note: %s" % e)
    log("save -> %s" % eal.save_loaded_asset(bp))
    cdo2 = unreal.get_default_object(eal.load_asset(ASSET).generated_class())
    mz = cdo2.get_editor_property("LevelMuzzleOffsets") or []
    fr = cdo2.get_editor_property("LevelFineRot") or []
    hs = cdo2.get_editor_property("LevelHoldScales") or []
    log("counts: Muzzle=%d FineRot=%d HoldScales=%d" % (len(mz), len(fr), len(hs)))
    for i, v in enumerate(list(mz)[:10]): log("  Muz  Mk%d=(%.1f,%.1f,%.1f)" % (i + 1, v.x, v.y, v.z))
    for i, r in enumerate(list(fr)[:10]): log("  Fine Mk%d=(P%.1f,Y%.1f,R%.1f)" % (i + 1, r.pitch, r.yaw, r.roll))
    ok = (len(mz) == 10 and len(fr) == 10 and len(hs) == 10)
    log("RESULT: %s" % ("OK" if ok else "PROBLEM"))
    return ok
try:
    ok = main(); log("Script " + ("SUCCEEDED" if ok else "FAILED")); sys.exit(0 if ok else 1)
except Exception as e:
    import traceback; log("Unhandled " + str(e)); traceback.print_exc(); sys.exit(1)
