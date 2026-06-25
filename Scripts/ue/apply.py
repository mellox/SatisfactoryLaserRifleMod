# apply.py -- finish what's reliably scriptable: recipe duration + 10-node research chain.
# IMPORTANT: set CDO defaults then SAVE (no post-set recompile -- recompiling a BP
# reverts the CDO to its stored bag and would wipe the values).
import unreal

EQ  = "/LaserRifleMod/Equipment/LaserRifle"
RES = "/LaserRifleMod/Research"
NODES_DIR = RES + "/Nodes"

eal   = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

def log(m):  unreal.log("LR_AP | " + str(m))
def warn(m): unreal.log_warning("LR_AP | " + str(m))

def bp_cdo(bp):
    return unreal.get_default_object(bp.generated_class())

def save(bp):
    eal.save_loaded_asset(bp)

def make_bp(name, pkg, parent_cls):
    full = pkg + "/" + name
    if eal.does_asset_exist(full): eal.delete_asset(full)
    f = unreal.BlueprintFactory(); f.set_editor_property("parent_class", parent_cls)
    return tools.create_asset(name, pkg, None, f)   # create_asset compiles once

# --- recipe duration (load the BLUEPRINT asset, not the _C class) ---------
rbp = eal.load_asset("/LaserRifleMod/Recipes/Recipe_LaserRifle")
if rbp:
    rc = bp_cdo(rbp)
    done = False
    for cand in ["mManufactoringDuration", "mManufacturingDuration"]:
        try:
            rc.set_editor_property(cand, 1.0); log("recipe duration via %s" % cand); done = True; break
        except Exception: pass
    if not done: warn("recipe duration property not found")
    save(rbp)

# --- can FGResearchTreeNode be a Blueprint parent? ------------------------
ncls = unreal.load_object(None, "/Script/FactoryGame.FGResearchTreeNode")
ncdo = unreal.get_default_object(ncls)
rel = sorted([a for a in dir(ncdo) if not a.startswith("__") and
        any(k in a.lower() for k in ["schematic", "position", "depend", "parent", "unlock"])])
log("FGResearchTreeNode relevant attrs: %s" % rel)

if not eal.does_directory_exist(NODES_DIR):
    eal.make_directory(NODES_DIR)

# --- build 10 node BPs ----------------------------------------------------
node_classes = []
prev_cls = None
for i in range(1, 11):
    sch = unreal.load_object(None, "/Script/LaserRifleMod.Schematic_LaserRifle_%02d" % i)
    try:
        nb = make_bp("Node_LaserRifle_%02d" % i, NODES_DIR, ncls)
    except Exception as e:
        warn("Mk%02d node create FAILED (FGResearchTreeNode may not be Blueprintable): %s" % (i, str(e)[:90]))
        node_classes = None; break
    nc = bp_cdo(nb)
    set_sch = False
    for p in ["mSchematics", "mSchematic"]:
        try:
            cur = nc.get_editor_property(p)
            nc.set_editor_property(p, [sch] if isinstance(cur, unreal.Array) else sch)
            set_sch = True; break
        except Exception: pass
    if prev_cls is not None:
        for p in ["mParents", "mParentDependencies", "mDependentNodes"]:
            try:
                cur = nc.get_editor_property(p)
                if isinstance(cur, unreal.Array):
                    nc.set_editor_property(p, [prev_cls]); break
            except Exception: pass
    for p in ["mUnlockPositioning", "mPosition", "mNodePosition"]:
        try:
            nc.set_editor_property(p, unreal.Vector2D(float(i - 1), 0.0)); break
        except Exception: pass
    save(nb)
    cls_path = "%s/Node_LaserRifle_%02d.Node_LaserRifle_%02d_C" % (NODES_DIR, i, i)
    nclsobj = unreal.load_object(None, cls_path)
    node_classes.append(nclsobj); prev_cls = nclsobj
    log("Mk%02d node created  schematic_set=%s" % (i, set_sch))

# --- wire the tree --------------------------------------------------------
tbp = eal.load_asset(RES + "/MAM_LaserRifle")
if tbp and node_classes:
    tc = bp_cdo(tbp)
    valid = [n for n in node_classes if n]
    try:
        tc.set_editor_property("mNodes", valid)
        back = tc.get_editor_property("mNodes")
        log("tree mNodes wired: %d/10" % len(back))
    except Exception as e:
        warn("mNodes set failed: %s" % str(e)[:90])
    save(tbp)

log("AP_RESULT=DONE")
