# slim_kit_pool.py -- empty BP_Equip_LaserRifle's KitComponentPool so the ~30 SK_Kit_*
# skeletal meshes (~6 GB, the PARKED lr.RandomComponents random-parts loadout) stop being
# cook dependencies. The header documents "Empty = feature no-ops" and the code no-ops on an
# empty pool, so this is zero runtime change -- it only removes dead cook weight (pak ~1.5GB
# -> ~200-400MB). The Components/Kit assets stay in the project for when the feature resumes.
# Run (game + editor CLOSED):
#   .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\ue\slim_kit_pool.py
import unreal, sys
P = "LR_SLIM"
def log(m):
    s = P + " " + str(m); print(s)
    try: unreal.log_warning(s)
    except Exception: pass
eal = unreal.EditorAssetLibrary
ASSET = "/LaserRifleMod/Equipment/LaserRifle/BP_Equip_LaserRifle"
def pool_len(cdo):
    v = cdo.get_editor_property("KitComponentPool")
    return len(v) if v is not None else 0
def main():
    log("=== slim_kit_pool START ===")
    bp = eal.load_asset(ASSET)
    if not bp:
        log("MISSING: " + ASSET); return False
    cdo = unreal.get_default_object(bp.generated_class())
    try:
        log("KitComponentPool before: %d entries" % pool_len(cdo))
        cdo.set_editor_property("KitComponentPool", [])
        try:
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        except Exception as e:
            log("compile_blueprint note: %s" % e)
        log("save -> %s" % eal.save_loaded_asset(bp))
        # readback from a fresh load to confirm it persisted
        cdo2 = unreal.get_default_object(eal.load_asset(ASSET).generated_class())
        n_after = pool_len(cdo2)
        log("KitComponentPool after: %d entries" % n_after)
        ok = (n_after == 0)
        log("RESULT: %s" % ("OK (pool emptied)" if ok else "MISMATCH (still populated)"))
        return ok
    except Exception as e:
        import traceback; log("FAILED: %s" % e); traceback.print_exc(); return False
try:
    ok = main(); log("Script " + ("SUCCEEDED" if ok else "FAILED")); sys.exit(0 if ok else 1)
except Exception as e:
    import traceback; log("Unhandled: " + str(e)); traceback.print_exc(); sys.exit(1)
