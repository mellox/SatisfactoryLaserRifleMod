# set_craft_durations.py -- set ONLY mManufactoringDuration on the existing rifle + Energy Cell recipe assets
# (no BP/desc/equip recreation), so the per-Mk visual tuning baked in the child BP CDOs (FineRot/muzzle/hold --
# NOT persisted in the finalize scripts, see laserrifle-state.md) is NOT disturbed. Run (game/build closed):
#   .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\ue\set_craft_durations.py
# Values (user 2026-07-13: "make the rifles x2 slower, cell at the rifle build speed"):
#   Rifle Mk N -> 2*(8 + 2*N)  (was 8+2*N: Mk1 10->20s ... Mk10 28->56s)
#   Energy Cell -> 20s (matches the new Mk1 rifle; slow hand-craft is a deliberate anti-over-craft, bulk = machine)
import unreal, sys
P="LR_DUR"
def log(m):
    s=P+" "+str(m); print(s)
    try: unreal.log_warning(s)
    except Exception: pass
eal=unreal.EditorAssetLibrary
def set_dur(asset_path, dur):
    a=eal.load_asset(asset_path)
    if not a: log("MISSING: "+asset_path); return False
    cdo=unreal.get_default_object(a.generated_class())
    try:
        cdo.set_editor_property("mManufactoringDuration", float(dur))
        eal.save_loaded_asset(a)
        rb=unreal.get_default_object(eal.load_asset(asset_path).generated_class()).get_editor_property("mManufactoringDuration")
        good=abs(float(rb)-float(dur))<0.01
        log("%s -> %.1fs (readback %.1f) %s" % (asset_path.rsplit('/',1)[-1], dur, rb, "OK" if good else "MISMATCH"))
        return good
    except Exception as e:
        log("FAILED %s: %s" % (asset_path, e)); return False
def main():
    log("====== set_craft_durations START ======")
    ok=True
    for n in range(1,11):
        ok = set_dur("/LaserRifleMod/Recipes/Recipe_LaserRifle_Mk%d" % n, 2*(8+2*n)) and ok
    ok = set_dur("/LaserRifleMod/Recipes/Recipe_LR_EnergyCell", 20.0) and ok
    log("====== DONE ok=%s ======" % ok)
    return ok
try:
    ok=main(); log("Script "+("SUCCEEDED" if ok else "FAILED")); sys.exit(0 if ok else 1)
except Exception as e:
    import traceback; log("Unhandled: "+str(e)); traceback.print_exc(); sys.exit(1)
