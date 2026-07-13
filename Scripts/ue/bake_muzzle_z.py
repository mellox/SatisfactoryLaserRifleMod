r"""
bake_muzzle_z.py -- lower the muzzle (beam-origin) HEIGHT on the Mks whose auto-derived muzzle sat too
high (front-slice center caught a tall top element), bringing them into the same Z band (~0.60 of body
height) as the Mks whose beams read right (Mk1/3/4/8). Keeps each muzzle's X (front) and Y unchanged --
only Z moves. Data-driven default; final polish is lr.MuzzleDZ in-game (these energy bodies have no
geometric barrel, so the exact bore is a visual call).

Run headless: .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\ue\bake_muzzle_z.py
"""
import unreal

EQ = "/LaserRifleMod/Equipment/LaserRifle"
PARENT_BP = EQ + "/BP_Equip_LaserRifle"
TARGET_FRAC = 0.60          # muzzle height as a fraction of body Z-extent (matches the well-behaved cluster)
FIX_MKS = [2, 5, 6, 7, 9, 10]
eal = unreal.EditorAssetLibrary

def log(m): unreal.log_warning("LR_MZBAKE | " + str(m))

def get_cdo(path):
    bp = eal.load_asset(path)
    if not bp: return None, None
    return bp, unreal.get_default_object(bp.generated_class())

pbp, pcdo = get_cdo(PARENT_BP)
if not pcdo:
    log("FATAL: parent BP not found"); raise SystemExit
meshes = list(pcdo.get_editor_property("LevelBodyMeshes"))
muz = list(pcdo.get_editor_property("LevelMuzzleOffsets"))

new_z = {}
for mk in FIX_MKS:
    mesh = meshes[mk - 1] if mk - 1 < len(meshes) else None
    cur = muz[mk - 1] if mk - 1 < len(muz) else None
    if not mesh or not cur:
        log("Mk%d: missing mesh/muzzle, skip" % mk); continue
    b = mesh.get_bounding_box()
    mn = b.get_editor_property("min").z; mx = b.get_editor_property("max").z
    nz = mn + TARGET_FRAC * (mx - mn)
    new_z[mk] = (cur.x, cur.y, nz, cur.z)
    log("Mk%d muzzle Z %.1f -> %.1f (frac->%.2f, X/Y kept)" % (mk, cur.z, nz, TARGET_FRAC))

def apply(path, label):
    bp, cdo = get_cdo(path)
    if not cdo: log("skip (not found): " + path); return
    m = list(cdo.get_editor_property("LevelMuzzleOffsets"))
    while len(m) < 10: m.append(unreal.Vector(0, 0, 0))
    for mk, (x, y, nz, _old) in new_z.items():
        m[mk - 1] = unreal.Vector(x, y, nz)
    cdo.set_editor_property("LevelMuzzleOffsets", m)
    try: unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    except Exception as e: log("%s compile warn: %s" % (label, e))
    unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
    log("%s: muzzle Z written for Mk%s" % (label, sorted(new_z.keys())))

apply(PARENT_BP, "parent")
for mk in sorted(new_z.keys()):
    apply(EQ + "/BP_Equip_LaserRifle_Mk%d" % mk, "child Mk%d" % mk)
log("DONE.")
