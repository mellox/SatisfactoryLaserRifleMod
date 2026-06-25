# find_vfx.py -- list Niagara/Cascade smoke/steam/exhaust effects in the game
# content so we can reuse one for the rifle's heat smoke. Run (game closed):
#   .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\ue\find_vfx.py
import unreal
ar = unreal.AssetRegistryHelpers.get_asset_registry()
KEYS = ("smoke","steam","vapor","vapour","exhaust","heat","fume","mist","gas")

def grab(pkg, cls):
    try:
        return ar.get_assets_by_class(unreal.TopLevelAssetPath(pkg, cls), True)
    except Exception as e:
        unreal.log_warning("LR_VFX | grab fail %s.%s: %s" % (pkg, cls, e))
        return []

cands = []
for pkg, cls in (("/Script/Niagara","NiagaraSystem"), ("/Script/Engine","ParticleSystem")):
    for a in grab(pkg, cls):
        name = str(a.asset_name).lower()
        path = str(a.package_name)
        if any(k in name for k in KEYS) and "/FactoryGame/" in path:
            cands.append((cls, path + "." + str(a.asset_name)))

cands = sorted(set(cands))
unreal.log_warning("LR_VFX | ==== %d smoke/steam candidates ====" % len(cands))
for cls, full in cands[:60]:
    unreal.log_warning("LR_VFX | [%s] %s" % ("NS" if "Niagara" in cls else "PS", full))
unreal.log_warning("LR_VFX | ==== end ====")
