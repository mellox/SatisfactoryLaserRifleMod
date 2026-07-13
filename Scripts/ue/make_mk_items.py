# make_mk_items.py -- SEPARATE-ITEM redesign: author 10 craftable Mk rifles.
# For each Mk 1..10:
#   - BP_Equip_LaserRifle_MkN : child of BP_Equip_LaserRifle, CDO FixedMkLevel=N (fixes the look/stats to that Mk)
#   - Desc_LaserRifle_MkN     : duplicate of Desc_LaserRifle, equipment=BP_Equip_MkN, per-Mk name + icon
#   - Recipe_LaserRifle_MkN   : duplicate of Recipe_LaserRifle; Mk1 from raw parts, MkN from a Mk(N-1) + parts
# The Mk schematics (C++) unlock Recipe_LaserRifle_MkN. Run (game/editor/build closed):
#   .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\ue\make_mk_items.py
import unreal, traceback, sys
P="LR_ITEMS"
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
ICON=EQ+"/Icons/T_LaserRifle_Icon_Mk%d"
DESC_TMPL=EQ+"/Desc_LaserRifle"
EQUIP_TMPL=EQ+"/BP_Equip_LaserRifle"
RECIPE_TMPL=RECDIR+"/Recipe_LaserRifle"

WIRE="/Game/FactoryGame/Resource/Parts/Wire/Desc_Wire.Desc_Wire_C"
IRON="/Game/FactoryGame/Resource/Parts/IronPlate/Desc_IronPlate.Desc_IronPlate_C"
QUARTZ="/Game/FactoryGame/Resource/Parts/QuartzCrystal/Desc_QuartzCrystal.Desc_QuartzCrystal_C"

# Base-game item descriptor paths for the Mk1-10 recipe ladder (recipe-pricing-research.md
# section 4). SEVERAL internal names are NON-OBVIOUS (Heat Sink=AluminumPlateReinforced,
# AI Limiter=CircuitBoardHighSpeed, Supercomputer=ComputerSuper, RCU=ModularFrameLightweight)
# -- resolve_item() below FAILS LOUDLY on any that don't load so a wrong path can't silently
# drop an ingredient (the old `if x:` guards hid exactly that).
PART = "/Game/FactoryGame/Resource/Parts/%s/Desc_%s.Desc_%s_C"
def _p(folder, name): return PART % (folder, name, name)
ITEMS = {
    "ReinforcedIronPlate": _p("IronPlateReinforced", "IronPlateReinforced"),
    "Wire":                WIRE,
    "Cable":               _p("Cable", "Cable"),
    "QuartzCrystal":       QUARTZ,
    "Rotor":               _p("Rotor", "Rotor"),
    "Quickwire":           _p("HighSpeedWire", "HighSpeedWire"),
    "CircuitBoard":        _p("CircuitBoard", "CircuitBoard"),
    "Stator":              _p("Stator", "Stator"),
    "CrystalOscillator":   _p("CrystalOscillator", "CrystalOscillator"),
    "Motor":               _p("Motor", "Motor"),
    "HeatSink":            _p("AluminumPlateReinforced", "AluminumPlateReinforced"),
    "AILimiter":           _p("CircuitBoardHighSpeed", "CircuitBoardHighSpeed"),
    "AluminumCasing":      _p("AluminumCasing", "AluminumCasing"),
    "Computer":            _p("Computer", "Computer"),
    "HeavyModularFrame":   _p("ModularFrameHeavy", "ModularFrameHeavy"),
    "HighSpeedConnector":  _p("HighSpeedConnector", "HighSpeedConnector"),
    "CoolingSystem":       _p("CoolingSystem", "CoolingSystem"),
    "Supercomputer":       _p("ComputerSuper", "ComputerSuper"),
    "RadioControlUnit":    _p("ModularFrameLightweight", "ModularFrameLightweight"),
}
# Per-Mk DELTA ingredients (each Mk N>=2 ALSO consumes 1x the prior Mk rifle, added in code --
# the vanilla Xeno-Basher "upgrade eats the previous tool" idiom). Mk10's optional Nuclear
# Pasta flourish from the doc is intentionally omitted (kept as baseline gear, not a spike).
RECIPE_LADDER = {
    1:  [("ReinforcedIronPlate",5),("Wire",40),("Cable",10),("QuartzCrystal",10)],
    2:  [("Rotor",3),("Cable",20),("Quickwire",20),("QuartzCrystal",15)],
    3:  [("CircuitBoard",4),("Quickwire",40),("QuartzCrystal",25)],
    4:  [("Stator",5),("CrystalOscillator",3),("Quickwire",60)],
    5:  [("Motor",4),("CircuitBoard",8),("CrystalOscillator",5)],
    6:  [("HeatSink",6),("AILimiter",4),("AluminumCasing",10)],
    7:  [("Computer",2),("HeatSink",10),("CrystalOscillator",8)],
    8:  [("HeavyModularFrame",3),("HighSpeedConnector",8),("Computer",3)],
    9:  [("CoolingSystem",6),("Supercomputer",2),("AILimiter",10)],
    10: [("RadioControlUnit",4),("CoolingSystem",10),("Supercomputer",4)],
}
_INGREDIENT_FAILURES = []
def resolve_item(key):
    cls = unreal.load_class(None, ITEMS[key])
    if not cls:
        _INGREDIENT_FAILURES.append((key, ITEMS[key]))
        err("INGREDIENT PATH FAILED to load: %s -> %s" % (key, ITEMS[key]))
    return cls

def load_class(path):
    return unreal.load_class(None, path)

def gen_class(bp_asset):
    try: return bp_asset.generated_class()
    except Exception: return None

def item_amount(item_cls, amount):
    ia=unreal.ItemAmount()
    ia.set_editor_property("item_class", item_cls)
    ia.set_editor_property("amount", int(amount))
    return ia

def probe():
    log("==== PROBE templates ====")
    for path in (DESC_TMPL, EQUIP_TMPL, RECIPE_TMPL):
        a=eal.load_asset(path)
        log("  %s : %s" % (path, ("OK class="+a.get_class().get_name()) if a else "MISSING"))
    d=eal.load_asset(DESC_TMPL)
    if d:
        cdo=unreal.get_default_object(d.generated_class())
        for prop in ("mEquipmentClass","mDisplayName","mSmallIcon","mPersistentBigIcon","mStackSize"):
            try: log("    Desc.%s = %s" % (prop, cdo.get_editor_property(prop)))
            except Exception as e: log("    Desc.%s ? %s" % (prop, e))
    r=eal.load_asset(RECIPE_TMPL)
    if r:
        cdo=unreal.get_default_object(r.generated_class())
        for prop in ("mProduct","mIngredients","mProducedIn","mManufactoringDuration"):
            try: log("    Recipe.%s = %s" % (prop, cdo.get_editor_property(prop)))
            except Exception as e: log("    Recipe.%s ? %s" % (prop, e))

def make_equip_bp(n):
    name="BP_Equip_LaserRifle_Mk%d"%n
    full=EQ+"/"+name
    if eal.does_asset_exist(full): eal.delete_asset(full)
    tmpl=eal.load_asset(EQUIP_TMPL)
    parent=gen_class(tmpl)
    if not parent: err("no equip template parent"); return None
    f=unreal.BlueprintFactory(); f.set_editor_property("parent_class", parent)
    bp=at.create_asset(name, EQ, unreal.Blueprint, f)
    if not bp: err("create equip "+name+" FAILED"); return None
    cdo=unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("FixedMkLevel", int(n))
    eal.save_loaded_asset(bp)
    rb=unreal.get_default_object(eal.load_asset(full).generated_class()).get_editor_property("FixedMkLevel")
    log("  equip %s FixedMkLevel=%s" % (name, rb))
    return bp

def make_desc(n, equip_bp):
    name="Desc_LaserRifle_Mk%d"%n
    full=EQ+"/"+name
    if eal.does_asset_exist(full): eal.delete_asset(full)
    dup=eal.duplicate_asset(DESC_TMPL, full)
    if not dup: err("dup desc "+name+" FAILED"); return None
    cdo=unreal.get_default_object(dup.generated_class())
    cdo.set_editor_property("mDisplayName", unreal.Text("Laser Rifle Mk.%d"%n))
    try: cdo.set_editor_property("mDescription", unreal.Text("Laser Rifle Mk.%d." % n))
    except Exception: pass
    eqc=gen_class(equip_bp)
    if eqc: cdo.set_editor_property("mEquipmentClass", eqc)
    icon=eal.load_asset(ICON%n)
    if icon:
        try:
            cdo.set_editor_property("mSmallIcon", icon)
            cdo.set_editor_property("mPersistentBigIcon", icon)
        except Exception as e: log("  desc icon warn: %s"%e)
    eal.save_loaded_asset(dup)
    log("  desc %s equip=%s icon=%s"%(name, eqc.get_name() if eqc else None, bool(icon)))
    return dup

def make_recipe(n, desc_bp, prev_desc_bp):
    name="Recipe_LaserRifle_Mk%d"%n
    full=RECDIR+"/"+name
    if eal.does_asset_exist(full): eal.delete_asset(full)
    dup=eal.duplicate_asset(RECIPE_TMPL, full)
    if not dup: err("dup recipe "+name+" FAILED"); return None
    cdo=unreal.get_default_object(dup.generated_class())
    descc=gen_class(desc_bp)
    cdo.set_editor_property("mProduct", [item_amount(descc, 1)])
    ings=[]
    # upgrade chain: MkN (N>=2) consumes 1x the prior Mk rifle (vanilla Xeno-Basher idiom)
    if n>=2 and prev_desc_bp:
        ings.append(item_amount(gen_class(prev_desc_bp), 1))
    # per-Mk DELTA ingredients from the pricing-doc ladder; resolve_item logs+records failures
    for key, amt in RECIPE_LADDER[n]:
        cls = resolve_item(key)
        if cls:
            ings.append(item_amount(cls, amt))
    cdo.set_editor_property("mIngredients", ings)
    # mProducedIn: set EXPLICITLY. The base template Recipe_LaserRifle now has an EMPTY mProducedIn -- the
    # leftover no-Mk rifle was retired by clearing it (see remove_base_rifle_recipe.py 2026-07-05), so a
    # fresh duplicate no longer INHERITS the workbench. Without this, re-generated Mk recipes would be
    # craftable NOWHERE (invisible in the Equipment Workshop). Fail LOUD if the workbench class won't load.
    wb = load_class("/Game/FactoryGame/Buildable/-Shared/WorkBench/BP_WorkshopComponent.BP_WorkshopComponent_C")
    if wb: cdo.set_editor_property("mProducedIn", [wb])
    else:  err("WorkBench class FAILED to load -- Mk%d recipe would be UNCRAFTABLE (no mProducedIn)!" % n)
    try: cdo.set_editor_property("mDisplayName", unreal.Text("Laser Rifle Mk.%d"%n))
    except Exception: pass
    try: cdo.set_editor_property("mManufactoringDuration", float(2*(8 + 2*n)))   # x2 slower (user 2026-07-13); was 8+2*n
    except Exception as e: log("  recipe dur warn: %s"%e)
    eal.save_loaded_asset(dup)
    log("  recipe %s product=%s ings=%d (%s) dur=%ds"%(
        name, descc.get_name() if descc else None, len(ings),
        ", ".join("%s x%d"%(k,a) for k,a in RECIPE_LADDER[n]), 2*(8+2*n)))
    return dup

def main():
    log("====== make_mk_items START ======")
    probe()
    equips={}; descs={}
    for n in range(1,11):
        e=make_equip_bp(n);  equips[n]=e
        d=make_desc(n, e);   descs[n]=d
    for n in range(1,11):
        make_recipe(n, descs[n], descs.get(n-1))
    if _INGREDIENT_FAILURES:
        err("==== %d INGREDIENT PATH(S) FAILED -- recipes shipped with MISSING ingredients! ====" % len(_INGREDIENT_FAILURES))
        for k, p in _INGREDIENT_FAILURES:
            err("    %s -> %s" % (k, p))
    else:
        log("All ladder ingredients resolved OK.")
    log("====== make_mk_items DONE ======")
    return not _INGREDIENT_FAILURES

try:
    ok=main(); log("Script "+("SUCCEEDED" if ok else "FAILED")); sys.exit(0)
except Exception as e:
    err("Unhandled: "+str(e)); traceback.print_exc(); sys.exit(1)
