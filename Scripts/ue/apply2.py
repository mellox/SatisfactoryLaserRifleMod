# apply2.py -- nail the research node schematic (mNodeSchematic) and learn the
# exact mNodes element type so we can populate the tree (or hand it off precisely).
import unreal

RES = "/LaserRifleMod/Research"
NODES_DIR = RES + "/Nodes"
eal = unreal.EditorAssetLibrary

def log(m):  unreal.log("LR_A2 | " + str(m))
def warn(m): unreal.log_warning("LR_A2 | " + str(m))

def bp_cdo(bp): return unreal.get_default_object(bp.generated_class())

# Full (trimmed) attr dump of the node CDO -> find schematic/position/parent props.
nb1 = eal.load_asset(NODES_DIR + "/Node_LaserRifle_01")
nc1 = bp_cdo(nb1)
allattrs = sorted([a for a in dir(nc1) if not a.startswith("__") and not a.startswith("get_") and not a.startswith("set_") and not a.startswith("call_")])
log("node CDO props (settable-ish): %s" % allattrs)

# Set mNodeSchematic on every node.
schematic_prop = None
for i in range(1, 11):
    nb = eal.load_asset(NODES_DIR + "/Node_LaserRifle_%02d" % i)
    nc = bp_cdo(nb)
    sch = unreal.load_object(None, "/Script/LaserRifleMod.Schematic_LaserRifle_%02d" % i)
    ok = False
    for p in ["mNodeSchematic", "node_schematic", "mSchematic"]:
        try:
            nc.set_editor_property(p, sch); ok = True; schematic_prop = p; break
        except Exception: pass
    eal.save_loaded_asset(nb)
    log("Mk%02d mNodeSchematic set=%s (via %s)" % (i, ok, schematic_prop))

# Learn mNodes element type, then try to populate.
tbp = eal.load_asset(RES + "/MAM_LaserRifle")
tc = bp_cdo(tbp)
arr = tc.get_editor_property("mNodes")
log("mNodes python type=%s repr=%s" % (type(arr).__name__, repr(arr)[:120]))

node_clss = [unreal.load_object(None, "%s/Node_LaserRifle_%02d.Node_LaserRifle_%02d_C" % (NODES_DIR, i, i)) for i in range(1, 11)]

# strategy A: typed unreal.Array of class
try:
    a = unreal.Array(unreal.Class)
    for n in node_clss: a.append(n)
    tc.set_editor_property("mNodes", a)
    log("mNodes set OK via Array(Class) -> len=%d" % len(tc.get_editor_property("mNodes")))
except Exception as e:
    warn("Array(Class) failed: %s" % str(e)[:110])
    # strategy B: append onto the live array
    try:
        live = tc.get_editor_property("mNodes")
        for n in node_clss: live.append(n)
        log("mNodes appended live -> len=%d" % len(tc.get_editor_property("mNodes")))
    except Exception as e2:
        warn("append live failed: %s" % str(e2)[:110])

eal.save_loaded_asset(tbp)
log("A2_RESULT=DONE")
