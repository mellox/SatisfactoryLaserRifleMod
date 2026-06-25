# verify.py -- fresh-session read-back of every scripted value, to confirm it
# persisted to disk (CDO writes can silently revert on save). Read-only.
import unreal

EQ  = "/LaserRifleMod/Equipment/LaserRifle"
RES = "/LaserRifleMod/Research"
eal = unreal.EditorAssetLibrary

def log(m): unreal.log("LR_V | " + str(m))

def cdo(path_no_c):
    bp = eal.load_asset(path_no_c)
    return unreal.get_default_object(bp.generated_class())

# ammo
a = cdo(EQ + "/Ammo_LaserRifle")
dmg = a.get_editor_property("mDamageTypesOnImpact")
damt = None
if len(dmg):
    for p in ["mDamageAmount", "DamageAmount", "Damage"]:
        try: damt = dmg[0].get_editor_property(p); break
        except Exception: pass
log("AMMO range=%s mag=%s fire=%s dmgElems=%d dmgAmount=%s" % (
    a.get_editor_property("mMaxAmmoEffectiveRange"),
    a.get_editor_property("mMagazineSize"),
    a.get_editor_property("mFireRate"), len(dmg), damt))

# descriptor
d = cdo(EQ + "/Desc_LaserRifle")
log("DESC name=%s equip=%s bigIcon=%s" % (
    d.get_editor_property("mDisplayName"),
    bool(d.get_editor_property("mEquipmentClass")),
    bool(d.get_editor_property("mPersistentBigIcon"))))

# recipe
r = cdo("/LaserRifleMod/Recipes/Recipe_LaserRifle")
prod = r.get_editor_property("mProduct")
log("RECIPE products=%d duration=%s ingredients=%d producedIn=%d" % (
    len(prod), r.get_editor_property("mManufactoringDuration"),
    len(r.get_editor_property("mIngredients")),
    len(r.get_editor_property("mProducedIn"))))

# weapon ammo wiring
w = cdo(EQ + "/BP_Equip_LaserRifle")
try: allowed = len(w.get_editor_property("mAllowedAmmoClasses"))
except Exception: allowed = "n/a"
log("WEAPON allowedAmmo=%s" % allowed)

# material params
mat = eal.load_asset(EQ + "/M_LaserRifle_Body")
params = unreal.MaterialEditingLibrary.get_scalar_parameter_names(mat) + \
         unreal.MaterialEditingLibrary.get_vector_parameter_names(mat)
log("MATERIAL params=%s" % params)

# research tree
t = cdo(RES + "/MAM_LaserRifle")
log("TREE mNodes=%d" % len(t.get_editor_property("mNodes")))

# meshes present
ok = sum(1 for L in ["a","i","b","h","e","c","d","k","l","r"]
         if eal.does_asset_exist(EQ + "/Meshes/blaster-%s" % L))
log("MESHES present=%d/10" % ok)

log("V_RESULT=DONE")
