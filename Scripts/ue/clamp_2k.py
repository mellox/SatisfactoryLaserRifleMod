# clamp_2k.py -- backup + clamp LIVE rifle textures to MaxTextureSize=2048 (a cook-time
# resolution cap; source mips are NOT destroyed). REVERSIBLE: each original .uasset is
# copied to _DevKit_Uncooked/LiveTex_4K_backup_<date>/ BEFORE it is edited, so reverting =
# copy the backup back over Content/ and re-cook.
# Reads the live list from scratchpad _slim_live.txt (Texture2D only); clamps those whose
# max dimension > 2048 (or whose size can't be read, to be safe). Makes NO other change.
# Run (game+editor CLOSED, under build lock): .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\ue\clamp_2k.py
import unreal, os, shutil, sys
P = "LR_CLAMP"
def log(m):
    print("%s %s" % (P, m))
    try: unreal.log_warning("%s %s" % (P, m))
    except Exception: pass

MOUNT     = "/LaserRifleMod"
CONTENT   = "C:/Claude/Projects/SatisfactoryModLoader/Mods/LaserRifleMod/Content"
BACKUP    = "C:/Claude/Projects/SatisfactoryModLoader/Mods/LaserRifleMod/_DevKit_Uncooked/LiveTex_4K_backup_2026-07-14"
LIVE_LIST = "C:/Users/mello/AppData/Local/Temp/claude/C--Claude-Projects-SatisfactoryLaserRifleMod/e80cb688-f841-4ea4-8ae3-5be39c0e75af/scratchpad/_slim_live.txt"
CAP = 2048
eal = unreal.EditorAssetLibrary

def content_file(pkg):
    rel = pkg[len(MOUNT):].lstrip("/")
    return os.path.join(CONTENT, rel) + ".uasset"

def tex_size(t):
    try:
        return int(t.blueprint_get_size_x()), int(t.blueprint_get_size_y())
    except Exception:
        pass
    try:
        ip = t.get_editor_property("imported_size")
        return int(ip.x), int(ip.y)
    except Exception:
        return 0, 0

targets = []
with open(LIVE_LIST) as f:
    for line in f:
        p = line.rstrip("\n").split("\t")
        if len(p) == 3 and p[1] == "Texture2D":
            targets.append(p[2])
log("live Texture2D candidates: %d" % len(targets))

clamped = skipped = backed = fails = 0
for pkg in targets:
    a = eal.load_asset(pkg)
    if not a:
        log("LOAD-FAIL %s" % pkg); fails += 1; continue
    x, y = tex_size(a); mx = max(x, y)
    if 0 < mx <= CAP:
        skipped += 1
        log("skip (%dx%d <= %d): %s" % (x, y, CAP, pkg.rsplit('/',1)[-1]))
        continue
    # backup original BEFORE edit
    src = content_file(pkg)
    dst = os.path.join(BACKUP, pkg[len(MOUNT):].lstrip("/")) + ".uasset"
    try:
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(src, dst); backed += 1
    except Exception as e:
        log("BACKUP-FAIL %s : %s" % (pkg, e)); fails += 1; continue
    # clamp
    try:
        a.set_editor_property("max_texture_size", CAP)
    except Exception as e:
        log("SETPROP-FAIL %s : %s" % (pkg, e)); fails += 1; continue
    ok = eal.save_loaded_asset(a)
    clamped += 1
    log("clamp %dx%d -> max %d  (%s)  saved=%s" % (x, y, CAP, pkg.rsplit('/',1)[-1], ok))

log("================ CLAMP RESULT ================")
log("clamped: %d   skipped(<=%d): %d   backed-up: %d   fails: %d" % (clamped, CAP, skipped, backed, fails))
log("backup dir: %s" % BACKUP)
log("RESULT: %s" % ("OK" if fails == 0 else "HAD-FAILS"))
log("DONE")
