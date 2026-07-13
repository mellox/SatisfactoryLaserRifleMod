r"""
export_bodies_muzzlecheck.py -- export each Mk's LevelBodyMeshes static mesh to FBX + dump its baked
LevelMuzzleOffsets and bounds, so Blender can mark the muzzle point on the exact mesh the game uses
(coords match because we export the UE-normalized mesh, not the pre-normalize source).

Run headless: .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\ue\export_bodies_muzzlecheck.py
"""
import unreal, os, json

EQ = "/LaserRifleMod/Equipment/LaserRifle"
PARENT_BP = EQ + "/BP_Equip_LaserRifle"
OUT = r"C:\Users\mello\AppData\Local\Temp\claude\C--Claude-Projects\d5a7ef55-e564-4109-aa3c-33547ce61346\scratchpad\muzzlecheck"
os.makedirs(OUT, exist_ok=True)
eal = unreal.EditorAssetLibrary

def log(m): unreal.log_warning("LR_MZEXPORT | " + str(m))

bp = eal.load_asset(PARENT_BP)
cdo = unreal.get_default_object(bp.generated_class())
meshes = list(cdo.get_editor_property("LevelBodyMeshes"))
muz = list(cdo.get_editor_property("LevelMuzzleOffsets"))

manifest = []
for i in range(10):
    mesh = meshes[i] if i < len(meshes) else None
    if not mesh:
        continue
    mk = i + 1
    fbx = os.path.join(OUT, "Mk%02d.fbx" % mk)
    task = unreal.AssetExportTask()
    task.object = mesh
    task.filename = fbx
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.options = unreal.FbxExportOption()
    ok = False
    try:
        ok = unreal.Exporter.run_asset_export_task(task)
    except Exception as e:
        log("Mk%d export EXC: %s" % (mk, e))
    b = mesh.get_bounding_box()
    mn = b.get_editor_property("min"); mx = b.get_editor_property("max")
    mo = muz[i] if i < len(muz) else None
    exists = os.path.exists(fbx)
    manifest.append({
        "mk": mk, "mesh": mesh.get_name(), "fbx": fbx, "ok": bool(ok) and exists,
        "muzzle": [mo.x, mo.y, mo.z] if mo else None,
        "bmin": [mn.x, mn.y, mn.z], "bmax": [mx.x, mx.y, mx.z]})
    log("Mk%d export=%s muzzle=%s" % (mk, exists, [round(mo.x,1),round(mo.y,1),round(mo.z,1)] if mo else None))

with open(os.path.join(OUT, "manifest.json"), "w") as f:
    json.dump(manifest, f, indent=1)
log("DONE %d meshes -> %s" % (len(manifest), OUT))
