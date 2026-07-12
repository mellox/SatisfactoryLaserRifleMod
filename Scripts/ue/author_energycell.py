# author_energycell.py -- author Desc_LR_EnergyCell (fresh plain UFGItemDescriptor) + Recipe_LR_EnergyCell.
# The Laser Rifle consumes ENERGY CELLS to reload (was the base-game Battery, which unlocks ~Tier 8 -- long
# after the Mk1 rifle, so a fresh early rifle bricked with no craftable fuel). The Energy Cell recipe unlocks
# WITH the Mk1 rifle schematic (see LaserRifleSchematics.cpp USchematic_LaserRifle_01 CellRecipeUnlock), so
# fuel is craftable exactly when the rifle is. Spec: _team/laserrifle-energycell-authoring.md.
# Run (game/editor/build CLOSED):  .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\ue\author_energycell.py
import unreal, traceback, sys
P="LR_CELL"
def log(m):
    s=P+" "+str(m); print(s)
    try: unreal.log_warning(s)
    except Exception: pass
def err(m):
    s=P+" ERROR: "+str(m); print(s, file=sys.stderr)
    try: unreal.log_error(s)
    except Exception: pass

eal=unreal.EditorAssetLibrary; at=unreal.AssetToolsHelpers.get_asset_tools()
EQ="/LaserRifleMod/Equipment/LaserRifle"
RECDIR="/LaserRifleMod/Recipes"
ICON=EQ+"/Icons/T_LaserRifle_Icon_Mk1"              # reuse an existing cooked texture for the first build

DESC_NAME="Desc_LR_EnergyCell";     DESC_FULL=EQ+"/"+DESC_NAME
RECIPE_NAME="Recipe_LR_EnergyCell"; RECIPE_FULL=RECDIR+"/"+RECIPE_NAME
RECIPE_TMPL=RECDIR+"/Recipe_LaserRifle"             # proven recipe template (same one make_mk_items clones)

ITEMDESC_PARENT="/Script/FactoryGame.FGItemDescriptor"
WIRE  ="/Game/FactoryGame/Resource/Parts/Wire/Desc_Wire.Desc_Wire_C"
QUARTZ="/Game/FactoryGame/Resource/Parts/QuartzCrystal/Desc_QuartzCrystal.Desc_QuartzCrystal_C"
WORKBENCH="/Game/FactoryGame/Buildable/-Shared/WorkBench/BP_WorkshopComponent.BP_WorkshopComponent_C"

def item_amount(cls, amt):
    ia=unreal.ItemAmount()
    ia.set_editor_property("item_class", cls)
    ia.set_editor_property("amount", int(amt))
    return ia

def _set(cdo, prop, value, label):
    # Fail-soft on a single property (esp. enum bindings, VERIFY-2 in the spec): log loudly, keep the
    # default, don't abort the whole authoring run (a wasted editor launch just to re-set one field).
    try:
        cdo.set_editor_property(prop, value)
    except Exception as e:
        err("set %s (%s) FAILED -> left default: %s" % (prop, label, e))

BATTERY_TMPL="/Game/FactoryGame/Resource/Parts/Battery/Desc_Battery"   # base-game item to clone

def make_desc():
    # Duplicate the base-game Battery descriptor (spec fallback b). A fresh UFGItemDescriptor BP required setting
    # mStackSize/mForm via unreal.EStackSize/EResourceForm, which are NOT Python-exposed in this engine
    # (`module 'unreal' has no attribute 'EStackSize'`), and those enums throw during ARGUMENT evaluation (before
    # the fail-soft _set() can catch them). Cloning Battery inherits a VALID solid form + stack size + icon + world
    # mesh already set, so we touch no enums at all -- we only relabel it and neutralise any generator-fuel energy
    # value. We KEEP Battery's world mesh (reads fine as a cell) but OVERRIDE the inventory icon with the rifle-
    # family Mk1 icon so the Energy Cell is visually DISTINCT from a base-game Battery in inventory / the "xN
    # spare" HUD (cold-review finding: an un-overridden clone looked identical to a Battery). A dedicated
    # T_LR_EnergyCell icon can overwrite mSmallIcon later with no path change.
    if eal.does_asset_exist(DESC_FULL): eal.delete_asset(DESC_FULL)
    dup=eal.duplicate_asset(BATTERY_TMPL, DESC_FULL)
    if not dup: err("dup Desc_Battery FAILED from "+BATTERY_TMPL); return None
    cdo=unreal.get_default_object(dup.generated_class())
    _set(cdo, "mDisplayName", unreal.Text("Energy Cell"), "name")
    _set(cdo, "mDescription", unreal.Text(
        "A rechargeable photonic energy cell. Loaded into the Laser Rifle to power its beam."), "desc")
    _set(cdo, "mEnergyValue", 0.0, "energyValue")   # not a generator fuel (fail-soft if the prop is absent)
    icon=eal.load_asset(ICON)
    if icon:
        _set(cdo, "mSmallIcon", icon, "smallIcon")
        _set(cdo, "mPersistentBigIcon", icon, "bigIcon")
    else:
        err("icon load MISSING (cell keeps the Battery icon -> looks like a Battery): "+ICON)
    eal.save_loaded_asset(dup)
    rb=unreal.get_default_object(eal.load_asset(DESC_FULL).generated_class())   # read-back proof
    log("desc %s name='%s' stack=%s form=%s icon=%s (from Desc_Battery dup)" % (
        DESC_NAME, rb.get_editor_property("mDisplayName"),
        rb.get_editor_property("mStackSize"), rb.get_editor_property("mForm"),
        bool(rb.get_editor_property("mSmallIcon"))))
    return dup

def make_recipe(desc_bp):
    if eal.does_asset_exist(RECIPE_FULL): eal.delete_asset(RECIPE_FULL)
    dup=eal.duplicate_asset(RECIPE_TMPL, RECIPE_FULL)      # proven clone path (make_mk_items.make_recipe)
    if not dup: err("dup recipe FAILED from "+RECIPE_TMPL); return None
    cdo=unreal.get_default_object(dup.generated_class())
    descc=desc_bp.generated_class()
    _set(cdo, "mProduct", [item_amount(descc, 1)], "product")
    wire=unreal.load_class(None, WIRE); quartz=unreal.load_class(None, QUARTZ)
    if not wire:   err("WIRE path FAILED: "+WIRE)
    if not quartz: err("QUARTZ path FAILED: "+QUARTZ)
    ings=[]
    if wire:   ings.append(item_amount(wire, 5))
    if quartz: ings.append(item_amount(quartz, 2))
    _set(cdo, "mIngredients", ings, "ingredients")
    wb=unreal.load_class(None, WORKBENCH)                  # base template's mProducedIn is EMPTY -> set explicitly
    if wb: _set(cdo, "mProducedIn", [wb], "producedIn")
    else:  err("WORKBENCH FAILED -> recipe UNCRAFTABLE (no mProducedIn): "+WORKBENCH)
    _set(cdo, "mManufactoringDuration", 2.0, "duration")  # NOTE header spelling: mManufactoringDuration
    _set(cdo, "mDisplayName", unreal.Text("Energy Cell"), "recipeName")
    eal.save_loaded_asset(dup)
    # READ-BACK proof (cold-review finding): mProduct/mIngredients go through fail-soft _set, so a silent set-
    # failure would leave the CLONE's inherited product = Desc_LaserRifle (crafting the "Energy Cell" recipe
    # would yield a RIFLE). Log the ACTUAL saved product class, not the intended local var, so that can't ship
    # silently. product0 MUST read Desc_LR_EnergyCell_C.
    rb=unreal.get_default_object(eal.load_asset(RECIPE_FULL).generated_class())
    rb_prod=rb.get_editor_property("mProduct")
    prod0=rb_prod[0].get_editor_property("item_class").get_name() if rb_prod else "NONE"
    rb_ings=rb.get_editor_property("mIngredients")
    if prod0 != "Desc_LR_EnergyCell_C":
        err("recipe product READ-BACK is '%s', expected Desc_LR_EnergyCell_C -- mProduct set was masked!" % prod0)
    log("recipe %s READBACK product0=%s ings=%d producedIn=%s dur=2.0" % (
        RECIPE_NAME, prod0, len(rb_ings), bool(wb)))
    return dup

def main():
    log("====== author_energycell START ======")
    d=make_desc()
    if not d: err("desc author FAILED -> aborting recipe"); return False
    r=make_recipe(d)
    ok = bool(r) and eal.does_asset_exist(DESC_FULL) and eal.does_asset_exist(RECIPE_FULL)
    log("assets exist: desc=%s recipe=%s" % (eal.does_asset_exist(DESC_FULL), eal.does_asset_exist(RECIPE_FULL)))
    log("====== author_energycell DONE ======")
    return ok

try:
    ok=main(); log("Script "+("SUCCEEDED" if ok else "FAILED")); sys.exit(0 if ok else 1)
except Exception as e:
    err("Unhandled: "+str(e)); traceback.print_exc(); sys.exit(1)
