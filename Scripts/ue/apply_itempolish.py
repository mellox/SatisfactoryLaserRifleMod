# apply_itempolish.py -- set CDO properties on the rifle descriptor + recipe assets.
# CDO-set-then-save, NO compile_blueprint afterwards (recompile reverts the CDO bag).
# Run headless via Scripts/ue/run_ue_python.ps1 (game closed, no build running).
import unreal
eal = unreal.EditorAssetLibrary

def log(m): unreal.log_warning("LR_ITEMPOLISH | " + m)

# --- Descriptor: non-stackable (SS_ONE) + tooltip name/description (Change 1 + 3b) ---
dbp = eal.load_asset("/LaserRifleMod/Equipment/LaserRifle/Desc_LaserRifle")
if dbp:
    dc = unreal.get_default_object(dbp.generated_class())
    set_ok = False
    # The Python binding names the enum 'StackSize' (readback showed <StackSize.SS_MEDIUM: 2>),
    # NOT 'EStackSize', and an int won't coerce into an EnumProperty — use the enum object.
    for form in ("unreal.StackSize.SS_ONE", "unreal.StackSize(0)"):
        try:
            dc.set_editor_property("mStackSize", eval(form)); set_ok = True; break
        except Exception as e:
            log("mStackSize via %s failed: %s" % (form, e))
    dc.set_editor_property("mDisplayName", unreal.Text("Laser Rifle"))
    dc.set_editor_property("mDescription", unreal.Text(
        "A directed-energy rifle that fires a focused photonic beam - no ammunition required. "
        "Upgradeable through the MAM from Mk.1 to Mk.10."))
    log("descriptor: stack set_ok=%s readback=%s" % (set_ok, dc.get_editor_property("mStackSize")))
    eal.save_loaded_asset(dbp)   # SAVE, do NOT compile_blueprint (would revert the CDO)
else:
    log("BLOCKER: Desc_LaserRifle not found")

# --- Recipe: longer craft time (Change 2). Note the CSS-typo'd property name. ---
rbp = eal.load_asset("/LaserRifleMod/Recipes/Recipe_LaserRifle")
if rbp:
    rc = unreal.get_default_object(rbp.generated_class())
    rc.set_editor_property("mManufactoringDuration", 24.0)   # typo'd name is the REAL one (3x the prior 8s)
    log("recipe: duration readback=%s" % rc.get_editor_property("mManufactoringDuration"))
    eal.save_loaded_asset(rbp)
else:
    log("BLOCKER: Recipe_LaserRifle not found")

log("DONE")
