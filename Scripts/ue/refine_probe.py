# refine_probe.py -- read-only: discover the exact property names we still need
# (recipe duration/ingredients/producedIn, desc stack-size enum + world mesh) and
# learn how vanilla research trees structure their nodes, so we can script or
# hand off the rest precisely.
import unreal

def log(m): unreal.log("LR_RP | " + str(m))

def dump(label, obj, keys):
    names = [a for a in dir(obj) if not a.startswith("__")]
    hits = [n for n in names if any(k in n.lower() for k in keys)]
    log("%s relevant attrs: %s" % (label, sorted(hits)))

# --- recipe ---------------------------------------------------------------
rec = unreal.load_object(None, "/LaserRifleMod/Recipes/Recipe_LaserRifle.Recipe_LaserRifle_C")
if rec:
    c = unreal.get_default_object(rec)
    dump("RECIPE", c, ["duration", "ingredient", "produce", "product", "manual", "manufact"])

# --- descriptor -----------------------------------------------------------
desc = unreal.load_object(None, "/LaserRifleMod/Equipment/LaserRifle/Desc_LaserRifle.Desc_LaserRifle_C")
if desc:
    c = unreal.get_default_object(desc)
    dump("DESC", c, ["stack", "mesh", "icon", "equipment", "category"])
    # enumerate EStackSize values
    try:
        vals = [v for v in dir(unreal.EStackSize) if v.startswith("SS_") or v.isupper()]
        log("EStackSize values: %s" % vals)
    except Exception as e:
        log("EStackSize introspection failed: %s" % e)

# --- research tree: learn node structure from vanilla -------------------
ar = unreal.AssetRegistryHelpers.get_asset_registry()
try:
    found = ar.get_assets_by_class(unreal.TopLevelAssetPath("/Script/FactoryGame", "FGResearchTree"), True)
except Exception:
    found = []
vanilla = [a for a in found if "LaserRifle" not in str(a.package_name)]
log("vanilla FGResearchTree count: %d" % len(vanilla))
if vanilla:
    a = vanilla[0]
    log("sample tree: %s" % a.package_name)
    tree = unreal.load_asset(str(a.package_name))
    cdo = unreal.get_default_object(tree.generated_class()) if hasattr(tree, "generated_class") else unreal.get_default_object(tree)
    try:
        nodes = cdo.get_editor_property("mNodes")
        log("  mNodes len=%d" % len(nodes))
        if len(nodes):
            n0 = nodes[0]
            log("  node[0] type=%s value=%s" % (type(n0).__name__, n0))
            # if it's a class, inspect a node CDO for schematic/positioning props
            try:
                ncdo = unreal.get_default_object(n0)
                dump("  NODE", ncdo, ["schematic", "position", "parent", "depend"])
            except Exception as e:
                log("  node cdo introspect failed: %s" % e)
    except Exception as e:
        log("  mNodes read failed: %s" % e)
else:
    log("no vanilla research trees in registry (FactoryGame content may not be mounted in this editor)")

# also: does a default node class exist?
for n in ["FGResearchTreeNode", "BP_ResearchTreeNode"]:
    o = unreal.load_object(None, "/Script/FactoryGame." + n)
    log("class /Script/FactoryGame.%s -> %s" % (n, "FOUND" if o else "missing"))

log("RP_RESULT=DONE")
