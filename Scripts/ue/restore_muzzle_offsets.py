# restore_muzzle_offsets.py -- put back the pre-zero per-Mk LevelMuzzleOffsets (the auto-derive
# experiment was a regression: it dropped the beam to mesh-center, below the barrel). These are
# the values read from the BP before zeroing (Mk2 precise from the runtime BEAMDIAG). They're
# "close but slightly low" for the re-worked meshes -- a per-Mk lr.MuzzleDZ raise perfects them.
# Run (game + editor CLOSED): .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\ue\restore_muzzle_offsets.py
import unreal, sys
P = "LR_RESTMUZ"
def log(m):
    print(P + " " + str(m))
    try: unreal.log_warning(P + " " + str(m))
    except Exception: pass
eal = unreal.EditorAssetLibrary
ASSET = "/LaserRifleMod/Equipment/LaserRifle/BP_Equip_LaserRifle"
# index 0 = Mk1 .. index 9 = Mk10
VALS = [(66,-9,6),(57.4,-0.1,5.6),(53,-1,4),(51,3,3),(53,-6,5),(62,-5,10),(55,0,7),(55,0,8),(55,-1,6),(59,4,3)]
def main():
    bp = eal.load_asset(ASSET)
    if not bp:
        log("MISSING " + ASSET); return False
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("LevelMuzzleOffsets", [unreal.Vector(x, y, z) for (x, y, z) in VALS])
    try:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    except Exception as e:
        log("compile note: %s" % e)
    log("save -> %s" % eal.save_loaded_asset(bp))
    cdo2 = unreal.get_default_object(eal.load_asset(ASSET).generated_class())
    a = cdo2.get_editor_property("LevelMuzzleOffsets")
    log("LevelMuzzleOffsets after: %d entries" % (len(a) if a else 0))
    for i, v in enumerate(list(a)[:10]):
        log("  Mk%d=(%.1f,%.1f,%.1f)" % (i + 1, v.x, v.y, v.z))
    fr = cdo2.get_editor_property("LevelFineRot"); hs = cdo2.get_editor_property("LevelHoldScales")
    log("(preserved) FineRot=%d HoldScales=%d" % (len(fr) if fr else 0, len(hs) if hs else 0))
    return (len(a) if a else 0) == 10
try:
    ok = main(); log("Script " + ("SUCCEEDED" if ok else "FAILED")); sys.exit(0 if ok else 1)
except Exception as e:
    import traceback; log("Unhandled " + str(e)); traceback.print_exc(); sys.exit(1)
