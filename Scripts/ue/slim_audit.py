# slim_audit.py -- READ-ONLY asset audit for package slimming (Phase 0).
# Walks HARD+SOFT dependencies from the mod's real shipping roots (schematics,
# recipes, descriptors, research, ammo, all BP_Equip_LaserRifle*, icons + the
# C++ ConstructorHelpers/LoadClass soft-ref paths) and reports which /LaserRifleMod
# assets are LIVE (reachable) vs DEAD (unreferenced -> cooked as dead weight because
# SML cooks the whole Content folder). Makes NO changes. Writes dead/live lists to
# the session scratchpad for Phase 1 to act on precisely.
# Run (game+editor CLOSED): .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\ue\slim_audit.py
import unreal, os, sys
from collections import deque, defaultdict
P = "LR_SLIM"
def log(m):
    print("%s %s" % (P, m))
    try: unreal.log_warning("%s %s" % (P, m))
    except Exception: pass

MOUNT   = "/LaserRifleMod"
CONTENT = "C:/Claude/Projects/SatisfactoryModLoader/Mods/LaserRifleMod/Content"
OUTDIR  = "C:/Users/mello/AppData/Local/Temp/claude/C--Claude-Projects-SatisfactoryLaserRifleMod/e80cb688-f841-4ea4-8ae3-5be39c0e75af/scratchpad"

ar = unreal.AssetRegistryHelpers.get_asset_registry()
try:
    ar.scan_paths_synchronous([MOUNT], force_rescan=True)
except Exception as e:
    log("scan note: %s" % e)

def cls_name(a):
    try: return str(a.asset_class_path.asset_name)
    except Exception:
        try: return str(a.asset_class)
        except Exception: return "?"

assets = ar.get_assets_by_path(MOUNT, recursive=True, include_only_on_disk_assets=True)
pkgs = {}
for a in assets:
    pkgs[str(a.package_name)] = cls_name(a)
log("assets under %s : %d" % (MOUNT, len(pkgs)))

def size_of(pn):
    rel = pn[len(MOUNT):].lstrip("/")
    for ext in (".uasset", ".umap"):
        f = os.path.join(CONTENT, rel) + ext
        try: return os.path.getsize(f)
        except Exception: pass
    return 0

# dependency options (attribute-set style is safest across engine versions)
dop = unreal.AssetRegistryDependencyOptions()
for attr, val in (("include_hard_package_references", True),
                  ("include_soft_package_references", True),
                  ("include_searchable_names", False),
                  ("include_soft_management_references", False),
                  ("include_hard_management_references", False)):
    try: setattr(dop, attr, val)
    except Exception as e: log("dop attr %s: %s" % (attr, e))

def deps(pn):
    try:
        d = ar.get_dependencies(unreal.Name(pn), dop)
        return [str(x) for x in (d or [])]
    except Exception:
        try:
            d = ar.get_dependencies(pn, dop)
            return [str(x) for x in (d or [])]
        except Exception:
            return []

def refs(pn):
    try:
        d = ar.get_referencers(unreal.Name(pn), dop)
        return [str(x) for x in (d or [])]
    except Exception:
        return []

TEX = {"Texture2D","TextureCube","VolumeTexture","Texture2DArray","TextureRenderTarget2D"}

# --- PROBE: prove dependency data is populated before trusting the walk ---
for probe in (MOUNT+"/Equipment/LaserRifle/BP_Equip_LaserRifle",
              MOUNT+"/Equipment/LaserRifle/Meshes_Tripo/M_Rifle_Mk01"):
    dd = deps(probe)
    log("PROBE deps(%s): %d" % (probe.rsplit("/",1)[-1], len(dd)))
    for d in dd[:14]: log("   dep> %s" % d)

# --- root set: real shipping entry points ---
CPP = [
 MOUNT+"/Equipment/LaserRifle/Rigged/SK_WireChain",
 MOUNT+"/Equipment/LaserRifle/Components/Kit/M_Kit_wire_sheathed",
 MOUNT+"/Equipment/LaserRifle/Rigged/SK_LaserRifle_Mk1c",
 MOUNT+"/Equipment/LaserRifle/Meshes_Tripo/M_Rifle_Mk01",
 MOUNT+"/Equipment/LaserRifle/M_LaserRifle_Beam",
 MOUNT+"/Equipment/LaserRifle/M_LaserRifle_Smoke",
 MOUNT+"/Audio/Play_LaserRifle_Fire",
 MOUNT+"/Equipment/LaserRifle/Desc_LR_EnergyCell",
 MOUNT+"/Equipment/LaserRifle/Ammo_LaserRifle",
]
roots = set()
for pn in pkgs:
    name = pn.rsplit("/",1)[-1]; low = pn.lower()
    if (name.startswith("BP_Equip_LaserRifle") or name.startswith("Desc_")
        or name.startswith("Recipe_") or name.startswith("Schematic_")
        or name.startswith("MAM_") or name.startswith("Ammo_")
        or name.startswith("BP_Unlock") or name.startswith("T_LaserRifle_Icon")
        or "/schematics/" in low or "/recipes/" in low or "/research/" in low
        or "/unlocks/" in low or "/icons/" in low):
        roots.add(pn)
for c in CPP:
    if c in pkgs: roots.add(c)
    else: log("ROOT-NOT-FOUND (C++ path absent!): %s" % c)
log("roots: %d" % len(roots))

# --- transitive closure (intra-mount) ---
live = set(roots); q = deque(roots)
while q:
    p = q.popleft()
    for d in deps(p):
        if d.startswith(MOUNT) and d not in live:
            live.add(d); q.append(d)
live_in = live & set(pkgs.keys())
dead = [p for p in pkgs if p not in live]

def mb(b): return b/1048576.0
live_sz = sum(size_of(p) for p in live_in)
dead_sz = sum(size_of(p) for p in dead)
log("================ RESULT ================")
log("LIVE(in-mount): %d files  %.1f MB" % (len(live_in), mb(live_sz)))
log("DEAD:           %d files  %.1f MB" % (len(dead), mb(dead_sz)))

g = defaultdict(lambda:[0,0])
for p in dead:
    folder = p.rsplit("/",1)[0]; s = size_of(p)
    g[folder][0]+=s; g[folder][1]+=1
log("=== DEAD by folder (MB, files) ===")
for folder,(s,n) in sorted(g.items(), key=lambda kv:-kv[1][0]):
    log("  %8.1f  %3d  %s" % (mb(s), n, folder[len(MOUNT):]))

gc = defaultdict(lambda:[0,0])
for p in dead:
    c = pkgs[p]; s=size_of(p); gc[c][0]+=s; gc[c][1]+=1
log("=== DEAD by class ===")
for c,(s,n) in sorted(gc.items(), key=lambda kv:-kv[1][0]):
    log("  %8.1f  %3d  %s" % (mb(s), n, c))

# self-consistency: a dead asset must NOT be referenced by a live asset
bad = []
for p in dead:
    for r in refs(p):
        if r in live and r.startswith(MOUNT): bad.append((p,r)); break
log("CONTRADICTIONS (dead referenced by live): %d" % len(bad))
for p,r in bad[:25]: log("  !! %s  <-  %s" % (p[len(MOUNT):], r[len(MOUNT):]))

live_tex = [p for p in live_in if pkgs[p] in TEX]
lt_sz = sum(size_of(p) for p in live_tex)
log("LIVE textures KEPT: %d  (%.1f MB)  <- these stay (no visual downgrade)" % (len(live_tex), mb(lt_sz)))

try:
    with open(os.path.join(OUTDIR,"_slim_dead.txt"),"w") as f:
        for p in sorted(dead): f.write("%d\t%s\t%s\n" % (size_of(p), pkgs[p], p))
    with open(os.path.join(OUTDIR,"_slim_live.txt"),"w") as f:
        for p in sorted(live_in): f.write("%d\t%s\t%s\n" % (size_of(p), pkgs[p], p))
    log("wrote _slim_dead.txt / _slim_live.txt to scratchpad")
except Exception as e:
    log("write err: %s" % e)
log("DONE")
