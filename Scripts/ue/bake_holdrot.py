r"""
bake_holdrot.py -- fold the user's in-game lr.HoldPitch/HoldYaw tuning into each Mk's baked
LevelFineRot. The CVars STACK on top of the baked value, so new = current_baked + dialed_delta.
Reads each Mk CHILD BP's current LevelFineRot (authoritative -- that's what the equipped rifle uses),
adds the delta, writes to the child + parent CDOs. Also REPORTS every Mk's LevelMuzzleOffsets vs its
mesh front-tip + final FineRot, so the muzzle-at-barrel situation can be assessed.

Run headless (game/build closed):
  .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\ue\bake_holdrot.py
"""
import unreal

EQ = "/LaserRifleMod/Equipment/LaserRifle"
PARENT_BP = EQ + "/BP_Equip_LaserRifle"
eal = unreal.EditorAssetLibrary

# (pitch, yaw, roll) deltas the user dialed in-game (lr.HoldPitch / lr.HoldYaw), to ADD to the baked value.
DELTAS = {
    1: (5.0, -2.0, 0.0),
    2: (-1.0, -5.0, 0.0),
    3: (4.0, 0.0, 0.0),
    4: (6.0, 0.0, 0.0),
    5: (3.0, 0.0, 0.0),
    8: (-1.0, 0.0, 0.0),
    9: (1.0, -6.0, 0.0),
}

def log(m): unreal.log_warning("LR_HOLDBAKE | " + str(m))

def get_cdo(path):
    bp = eal.load_asset(path)
    if not bp: return None, None
    return bp, unreal.get_default_object(bp.generated_class())

def fr_list(cdo):
    fr = list(cdo.get_editor_property("LevelFineRot"))
    while len(fr) < 10: fr.append(unreal.Rotator(0.0, 0.0, 0.0))
    return fr

pbp, pcdo = get_cdo(PARENT_BP)
if not pcdo:
    log("FATAL: parent BP not found"); raise SystemExit
parent_fr = fr_list(pcdo)

# Compute new absolute values from each CHILD's current baked FineRot (authoritative).
new_vals = {}
for mk, (dp, dy, dr) in DELTAS.items():
    cbp, ccdo = get_cdo(EQ + "/BP_Equip_LaserRifle_Mk%d" % mk)
    base = fr_list(ccdo)[mk - 1] if ccdo else parent_fr[mk - 1]
    np_, ny_, nr_ = base.pitch + dp, base.yaw + dy, base.roll + dr
    new_vals[mk] = (np_, ny_, nr_)
    log("Mk%d: baked(P%.1f Y%.1f R%.1f) + dialed(P%.1f Y%.1f R%.1f) = (P%.1f Y%.1f R%.1f)"
        % (mk, base.pitch, base.yaw, base.roll, dp, dy, dr, np_, ny_, nr_))

def apply(path, label):
    bp, cdo = get_cdo(path)
    if not cdo: log("skip (not found): " + path); return
    fr = fr_list(cdo)
    for mk, (np_, ny_, nr_) in new_vals.items():
        fr[mk - 1] = unreal.Rotator(roll=nr_, pitch=np_, yaw=ny_)
    cdo.set_editor_property("LevelFineRot", fr)
    try: unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    except Exception as e: log("%s compile warn: %s" % (label, e))
    unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
    log("%s: FineRot written for Mk%s" % (label, sorted(new_vals.keys())))

apply(PARENT_BP, "parent")
for mk in sorted(new_vals.keys()):
    apply(EQ + "/BP_Equip_LaserRifle_Mk%d" % mk, "child Mk%d" % mk)

# --- MUZZLE / FINEROT REPORT for all 10 Mk (muzzleOffset vs mesh geometric front-tip) ---
_, pcdo2 = get_cdo(PARENT_BP)
muz = list(pcdo2.get_editor_property("LevelMuzzleOffsets"))
meshes = list(pcdo2.get_editor_property("LevelBodyMeshes"))
frA = fr_list(pcdo2)
log("==== MUZZLE / FINEROT REPORT (all Mk) ====")
for i in range(10):
    mk = i + 1
    mo = muz[i] if i < len(muz) else None
    mesh = meshes[i] if i < len(meshes) else None
    tip = None
    if mesh:
        b = mesh.get_bounding_box()
        mn = b.get_editor_property("min"); mx = b.get_editor_property("max")
        tip = (mx.x, (mn.y + mx.y) / 2.0, (mn.z + mx.z) / 2.0)
    dev = None
    if mo and tip and not (abs(mo.x) < 0.01 and abs(mo.y) < 0.01 and abs(mo.z) < 0.01):
        dev = ((mo.x - tip[0])**2 + (mo.y - tip[1])**2 + (mo.z - tip[2])**2) ** 0.5
    log("Mk%-2d mesh=%-18s muzzle=%-22s frontTip=%-22s dev=%s fineRot=(P%.1f Y%.1f R%.1f)" % (
        mk, mesh.get_name() if mesh else "None",
        ("(%.1f,%.1f,%.1f)" % (mo.x, mo.y, mo.z)) if mo else "None",
        ("(%.1f,%.1f,%.1f)" % tip) if tip else "None",
        ("%.1fcm" % dev) if dev is not None else ("ZERO->bboxTip" if (mo and not tip) else "-"),
        frA[i].pitch, frA[i].yaw, frA[i].roll))
log("DONE.")
