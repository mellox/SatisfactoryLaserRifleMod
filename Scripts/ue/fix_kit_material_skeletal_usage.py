r"""
fix_kit_material_skeletal_usage.py -- set bUsedWithSkeletalMesh on every material the
skeletal kit-part/wire system renders with. The M_Kit_* materials were authored for STATIC
meshes; a material without the skeletal usage flag silently falls back to the engine default
gray in a cooked/Shipping build (user-observed: all parts + wires untextured after the
skeletal-pool pass). Also flags the rifle body material (used on the SK_WireChain connector
wires) for the same reason.

Run (game/editor/build closed):
  .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\ue\fix_kit_material_skeletal_usage.py
"""
import unreal

EQ = "/LaserRifleMod/Equipment/LaserRifle"
KIT_DIR = EQ + "/Components/Kit"
PARENT_BP = EQ + "/BP_Equip_LaserRifle"

eal = unreal.EditorAssetLibrary

def log(m): unreal.log_warning("LR_MATFLAG | " + str(m))

def flag(mat_path):
    mat = eal.load_asset(mat_path)
    if not mat:
        log("MISSING " + mat_path)
        return False
    base = mat
    # material instances: flag the parent material (usage lives on the base UMaterial)
    while isinstance(base, unreal.MaterialInstance):
        base = base.get_editor_property("parent")
        if not base:
            log("no base material for " + mat_path)
            return False
    try:
        already = base.get_editor_property("used_with_skeletal_mesh")
    except Exception as e:
        log("cannot read usage on %s: %s" % (base.get_name(), e))
        return False
    if already:
        log("already flagged: " + base.get_name())
        return True
    base.set_editor_property("used_with_skeletal_mesh", True)
    saved = eal.save_asset(base.get_path_name().split(".")[0])
    log("flagged used_with_skeletal_mesh: %s saved=%s" % (base.get_name(), saved))
    return True

count = 0
for path in eal.list_assets(KIT_DIR, recursive=False, include_folder=False):
    obj = path.split(".")[0]
    short = obj.rsplit("/", 1)[-1]
    if short.startswith("M_Kit_"):
        if flag(obj):
            count += 1
log("kit materials flagged: %d" % count)

# rifle body material (assigned to the SK_WireChain connector wires at BeginPlay)
bp = eal.load_asset(PARENT_BP)
if bp:
    cdo = unreal.get_default_object(bp.generated_class())
    try:
        body_mat = cdo.get_editor_property("BodyMaterial")
        if body_mat:
            flag(body_mat.get_path_name().split(".")[0])
        else:
            log("BodyMaterial on CDO is None -- wires fall back to BodyMesh material 0 at "
                "runtime; flag that material manually if wires stay gray")
    except Exception as e:
        log("BodyMaterial read failed: %s" % e)
log("DONE.")
