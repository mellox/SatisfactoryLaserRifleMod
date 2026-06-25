# discover.py -- read-only: confirm which parent classes resolve before we
# create any gameplay assets (avoid authoring subtly-broken binaries).
import unreal

def log(m): unreal.log("LR_DISC | " + str(m))

def probe(path):
    try:
        obj = unreal.load_object(None, path)
    except Exception as e:
        obj = None
    log("%-55s -> %s" % (path, ("FOUND" if obj else "missing")))
    return obj

log("--- ammo candidates ---")
for n in ["FGAmmoType", "FGAmmoTypeLaser", "FGAmmoTypeProjectile",
          "FGAmmoTypeInstantHit", "FGAmmoTypeSpreadShot"]:
    probe("/Script/FactoryGame." + n)

log("--- weapon / descriptor / recipe / research ---")
for n in ["FGWeapon", "FGEquipmentDescriptor", "FGRecipe",
          "FGResearchTree", "FGSchematic", "FGSchematicCategory"]:
    probe("/Script/FactoryGame." + n)

log("--- our mod classes (need editor DLL loaded) ---")
for n in ["Schematic_LaserRifle_01", "Schematic_LaserRifle_10",
          "Cat_LaserRifle", "LaserRifleSubsystem"]:
    probe("/Script/LaserRifleMod." + n)

# Inspect the ammo base for the exact property names the checklist references.
amm = unreal.load_object(None, "/Script/FactoryGame.FGAmmoType")
if amm:
    want = ["mDamageTypesOnImpact", "mMaxAmmoEffectiveRange", "mMagazineSize",
            "mFireRate", "mAmmoDamageFalloff"]
    log("--- FGAmmoType property presence ---")
    for p in want:
        has = unreal.EditorAssetLibrary  # placeholder
        try:
            # property existence via the class' default object
            cdo = unreal.get_default_object(amm)
            val = cdo.get_editor_property(p)
            log("  %-28s present (sample=%s)" % (p, type(val).__name__))
        except Exception as e:
            log("  %-28s ABSENT/diff-name (%s)" % (p, str(e)[:60]))

log("DISC_RESULT=DONE")
