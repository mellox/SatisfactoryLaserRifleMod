# remove_base_rifle_recipe.py -- retire the leftover no-Mk "Laser Rifle" from the crafting menu.
#
# Recipe_LaserRifle / Desc_LaserRifle are TEMPLATES (make_mk_items.py duplicates them into the 10 Mk items).
# Nothing unlocks the base recipe (the C++ schematics unlock Recipe_LaserRifle_MkN), yet the base "Laser
# Rifle" shows in the Equipment Workshop -- a leftover. UFGRecipe has NO unlock flag; a recipe appears where
# its mProducedIn lists a workbench. So: CLEAR the base recipe's mProducedIn -> craftable nowhere -> gone.
# The already-generated Recipe_LaserRifle_MkN assets keep their OWN mProducedIn, so they're UNAFFECTED.
# (Follow-up handled separately: make_mk_items.py should SET mProducedIn per-Mk so a future re-run from this
#  now-empty template doesn't inherit an empty list.)  Run (game/editor/build CLOSED):
#   .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\ue\remove_base_rifle_recipe.py
import unreal
def log(m):
    s = "LR_REMOVE " + str(m); print(s)
    try: unreal.log_warning(s)
    except Exception: pass
eal = unreal.EditorAssetLibrary
BASE = "/LaserRifleMod/Recipes/Recipe_LaserRifle"
MK1  = "/LaserRifleMod/Recipes/Recipe_LaserRifle_Mk1"

def dump(path):
    a = eal.load_asset(path)
    if not a:
        log("  MISSING: " + path); return None
    gc = a.generated_class()
    cdo = unreal.get_default_object(gc)
    for prop in ("mProduct", "mProducedIn", "mDisplayName"):
        try: log("  %s.%s = %s" % (path.rsplit('/', 1)[-1], prop, cdo.get_editor_property(prop)))
        except Exception as e: log("  %s.%s ? %s" % (path.rsplit('/', 1)[-1], prop, e))
    try: log("  %s super = %s" % (path.rsplit('/', 1)[-1], gc.get_super_class().get_name()))
    except Exception: pass
    return a, gc, cdo

log("==== PROBE (compare Mk1 vs base) ====")
dump(MK1)
res = dump(BASE)
if res:
    a, gc, cdo = res
    before = cdo.get_editor_property("mProducedIn")
    n0 = len(before) if before else 0
    log("BASE mProducedIn BEFORE len=%d -> %s" % (n0, before))
    cdo.set_editor_property("mProducedIn", [])
    ok = eal.save_loaded_asset(a)
    after = unreal.get_default_object(eal.load_asset(BASE).generated_class()).get_editor_property("mProducedIn")
    n1 = len(after) if after else 0
    log("BASE mProducedIn AFTER  len=%d (saved=%s) -- base recipe now craftable NOWHERE" % (n1, ok))
    if n1 != 0:
        log("WARNING: mProducedIn NOT cleared -- fix did not persist, investigate!")
else:
    log("ERROR: base recipe asset not found at " + BASE)
log("==== DONE ====")
