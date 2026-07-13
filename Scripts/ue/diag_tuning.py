# diag_tuning.py -- READ-ONLY: dump the per-Mk tuning arrays (LevelMuzzleOffsets / LevelFineRot /
# LevelHoldScales) + KitComponentPool from the parent BP and each child BP CDO, to see where the
# tuning lives and whether it survived. No writes.
import unreal, sys
def log(m):
    print("LR_DIAG " + str(m))
    try: unreal.log_warning("LR_DIAG " + str(m))
    except Exception: pass
eal = unreal.EditorAssetLibrary
def fv(v):
    try: return "(%.0f,%.0f,%.0f)" % (v.x, v.y, v.z)
    except Exception: return str(v)
def fr(r):
    try: return "(P%.1f,Y%.1f,R%.1f)" % (r.pitch, r.yaw, r.roll)
    except Exception: return str(r)
def dump(path):
    bp = eal.load_asset(path)
    if not bp:
        log(path + " MISSING"); return
    cdo = unreal.get_default_object(bp.generated_class())
    name = path.rsplit('/', 1)[-1]
    for prop, fmt in (("LevelMuzzleOffsets", fv), ("LevelFineRot", fr), ("LevelHoldScales", str)):
        try:
            v = cdo.get_editor_property(prop)
            lst = list(v) if v is not None else []
            body = " ".join(fmt(x) for x in lst[:11])
            log("%s.%s = %d  [%s]" % (name, prop, len(lst), body))
        except Exception as e:
            log("%s.%s ERR %s" % (name, prop, e))
    try:
        kp = cdo.get_editor_property("KitComponentPool")
        log("%s.KitComponentPool = %d" % (name, len(kp) if kp is not None else 0))
    except Exception as e:
        log("%s.KitComponentPool ERR %s" % (name, e))
B = "/LaserRifleMod/Equipment/LaserRifle/"
for p in [B+"BP_Equip_LaserRifle", B+"BP_Equip_LaserRifle_Mk1", B+"BP_Equip_LaserRifle_Mk2",
          B+"BP_Equip_LaserRifle_Mk6", B+"BP_Equip_LaserRifle_Mk10"]:
    dump(p)
log("DONE")
sys.exit(0)
