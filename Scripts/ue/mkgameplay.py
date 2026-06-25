# mkgameplay.py -- checklist steps 7-16 (ammo, weapon shell, descriptor, recipe, research tree).
# Creates asset shells at the EXACT required paths with correct parents, sets the
# confirmed-safe defaults, attempts the fragile bits in guarded blocks, and READS
# BACK every value so we know exactly what stuck vs. what needs hand-authoring.
import unreal

EQ  = "/LaserRifleMod/Equipment/LaserRifle"
RES = "/LaserRifleMod/Research"

eal   = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

def log(m):  unreal.log("LR_GP | " + str(m))
def warn(m): unreal.log_warning("LR_GP | " + str(m))

def make_bp(name, pkg, parent_script_path):
    cls = unreal.load_object(None, parent_script_path)
    if not cls:
        warn("parent missing: %s (skip %s)" % (parent_script_path, name)); return None
    full = pkg + "/" + name
    if eal.does_asset_exist(full):
        eal.delete_asset(full)
    f = unreal.BlueprintFactory()
    f.set_editor_property("parent_class", cls)
    bp = tools.create_asset(name, pkg, None, f)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    log("created %s (parent %s)" % (full, parent_script_path.split('.')[-1]))
    return bp

def cdo_of(bp):
    return unreal.get_default_object(bp.generated_class())

def tset(cdo, prop, value):
    try:
        cdo.set_editor_property(prop, value)
        back = cdo.get_editor_property(prop)
        log("    %-26s := %s  (readback=%s)" % (prop, value, back))
        return True
    except Exception as e:
        warn("    %-26s FAILED: %s" % (prop, str(e)[:90]))
        return False

def save(bp):
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    eal.save_loaded_asset(bp)

# =====================================================================
# 7-8  Ammo_LaserRifle  (parent FGAmmoTypeLaser) -- EXACT path required
# =====================================================================
log("=== Ammo_LaserRifle ===")
ammo = make_bp("Ammo_LaserRifle", EQ, "/Script/FactoryGame.FGAmmoTypeLaser")
if ammo:
    c = cdo_of(ammo)
    tset(c, "mMaxAmmoEffectiveRange", 10000.0)
    tset(c, "mMagazineSize", 25)
    tset(c, "mFireRate", 6.0)
    # damage element: instanced UFGDamageType with mDamageAmount = 10 (Mk1 base)
    try:
        dcls = unreal.load_object(None, "/Script/FactoryGame.FGDamageType")
        dmg = unreal.new_object(dcls, c)            # may not be supported
        for cand in ["mDamageAmount", "DamageAmount", "Damage"]:
            try: dmg.set_editor_property(cand, 10.0); log("    damage.%s=10" % cand); break
            except Exception: pass
        tset(c, "mDamageTypesOnImpact", [dmg])
    except Exception as e:
        warn("    damage element NOT scripted (add 1 element by hand): %s" % str(e)[:90])
    save(ammo)

# =====================================================================
# 9  BP_Equip_LaserRifle shell (parent FGWeapon) -- components+graph by hand
# =====================================================================
log("=== BP_Equip_LaserRifle (shell) ===")
weap = make_bp("BP_Equip_LaserRifle", EQ, "/Script/FactoryGame.FGWeapon")
if weap:
    c = cdo_of(weap)
    # try to point ammo at our Ammo_LaserRifle (best-known property names)
    ammo_cls = unreal.load_object(None, EQ + "/Ammo_LaserRifle.Ammo_LaserRifle_C")
    if ammo_cls:
        for prop in ["mDefaultAmmoClass", "mAllowedAmmoClasses", "mAmmoClass", "mMagazineAmmoClass"]:
            try:
                cur = c.get_editor_property(prop)
                val = [ammo_cls] if isinstance(cur, unreal.Array) else ammo_cls
                tset(c, prop, val)
            except Exception:
                pass
    save(weap)

# =====================================================================
# 13  Desc_LaserRifle (parent FGEquipmentDescriptor)
# =====================================================================
log("=== Desc_LaserRifle ===")
desc = make_bp("Desc_LaserRifle", EQ, "/Script/FactoryGame.FGEquipmentDescriptor")
if desc:
    c = cdo_of(desc)
    tset(c, "mDisplayName", unreal.Text("Laser Rifle"))
    tset(c, "mDescription", unreal.Text("Craftable hitscan laser rifle with a Mk1-Mk10 research progression."))
    try: tset(c, "mStackSize", unreal.EStackSize.SS_ONE)
    except Exception: warn("    mStackSize enum value name differs")
    if weap:
        wcls = unreal.load_object(None, EQ + "/BP_Equip_LaserRifle.BP_Equip_LaserRifle_C")
        if wcls: tset(c, "mEquipmentClass", wcls)
    # icon
    icon = eal.load_asset(EQ + "/Icons/T_LaserRifle_Icon_Mk1")
    if icon:
        for p in ["mPersistentBigIcon", "mSmallIcon"]:
            tset(c, p, icon)
    save(desc)

# =====================================================================
# 14  Recipe_LaserRifle (parent FGRecipe)
# =====================================================================
log("=== Recipe_LaserRifle ===")
rec = make_bp("Recipe_LaserRifle", "/LaserRifleMod/Recipes", "/Script/FactoryGame.FGRecipe")
if rec:
    c = cdo_of(rec)
    if desc:
        dcls = unreal.load_object(None, EQ + "/Desc_LaserRifle.Desc_LaserRifle_C")
        try:
            ia = unreal.ItemAmount()
            ia.set_editor_property("item_class", dcls)
            ia.set_editor_property("amount", 1)
            tset(c, "mProduct", [ia])
        except Exception as e:
            warn("    mProduct struct set failed (set by hand): %s" % str(e)[:90])
    tset(c, "mManufacturingDuration", 1.0)
    save(rec)

# =====================================================================
# 15-16  MAM_LaserRifle research tree (parent FGResearchTree) -- EXACT path
# =====================================================================
log("=== MAM_LaserRifle (research tree shell) ===")
tree = make_bp("MAM_LaserRifle", RES, "/Script/FactoryGame.FGResearchTree")
if tree:
    c = cdo_of(tree)
    # report the node-related property names so we know how to wire 10 nodes
    for p in ["mNodes", "mResearchTreeNodeWidgetClass", "mUnlockTreeName", "mAssetDisplayText"]:
        try:
            v = c.get_editor_property(p)
            log("    tree prop %-30s = %s (%s)" % (p, v, type(v).__name__))
        except Exception:
            warn("    tree prop %-30s ABSENT" % p)
    save(tree)

log("GP_RESULT=DONE")
