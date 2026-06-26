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
    wire=load_class(WIRE); iron=load_class(IRON); quartz=load_class(QUARTZ)
    ings=[]
    if n>=2 and prev_desc_bp:
        ings.append(item_amount(gen_class(prev_desc_bp), 1))   # upgrade chain: consume the previous Mk
    if wire:   ings.append(item_amount(wire,   25*n))
    if iron:   ings.append(item_amount(iron,   10*n))
    if quartz and n>=2: ings.append(item_amount(quartz, 5*n))
    cdo.set_editor_property("mIngredients", ings)
    try: cdo.set_editor_property("mDisplayName", unreal.Text("Laser Rifle Mk.%d"%n))
    except Exception: pass
    try: cdo.set_editor_property("mManufactoringDuration", float(8 + 2*n))
    except Exception as e: log("  recipe dur warn: %s"%e)
    eal.save_loaded_asset(dup)
    log("  recipe %s product=%s ings=%d dur=%ds"%(name, descc.get_name() if descc else None, len(ings), 8+2*n))
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
    log("====== make_mk_items DONE ======")
    return True

try:
    ok=main(); log("Script "+("SUCCEEDED" if ok else "FAILED")); sys.exit(0)
except Exception as e:
    err("Unhandled: "+str(e)); traceback.print_exc(); sys.exit(1)
