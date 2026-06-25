# create_mam_tree.py  (LaserRifleMod)  -- builds TWO MAM research trees:
#   MAM_LaserRifle          : single column, the Mk + damage-line chain.
#                             Mk1 -> Dmg I..V -> Mk2 -> Mk3 ... -> Mk10
#                             (completing a Mk's damage line gates the next Mk,
#                              because each node's parent is the previous node).
#   MAM_LaserRifle_Systems  : two columns, Heat Capacity chain + Cooling chain.
#
# Nodes are base-game BPD_ResearchTreeNode_C with a fully populated
# mNodeDataStruct (schematic + grid coords + parent/child/unhide chain).
# Run (game + editor closed):
#   .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\ue\create_mam_tree.py
import unreal, sys, os, traceback

P="LR_MAM"
def log(m):
    s=P+" "+str(m); print(s)
    try: unreal.log_warning(s)
    except Exception: pass
def err(m):
    s=P+" ERROR: "+str(m); print(s, file=sys.stderr)
    try: unreal.log_error(s)
    except Exception: pass

PKG_DIR="/LaserRifleMod/Research"
NODE_BP_CLASS_PATH="/Game/FactoryGame/Schematics/Research/BPD_ResearchTreeNode.BPD_ResearchTreeNode_C"
RT_PARENT_CLASS="/Script/FactoryGame.FGResearchTree"

# GUID-suffixed fields of the engine MAMTree_NodeData_Struct (verified values).
F_SCH ="Schematic_27_3663A42446FDB4387D0C81AFC23E225B"
F_CRD ="Coordinates_23_5A3DE6C040C7026EDEA49E9CE8612292"
F_PAR ="Parents_20_7A099B96409362536B743BA1CC77C234"
F_CHR ="ChildrenAndRoads_34_758C9E0D4F09DAF4BBAD309358952A0A"
F_UNH ="UnhiddenBy_38_909B07D7461225A33C48A68A7139FE63"
F_NTU ="NodesToUnhide_33_A6E465554D49C98EE2A0ECB493BE5CBA"
CX="X_2_3FF107F84D30EB52DD50898C7D2CAD67"
CY="Y_4_F18C5B824136D7759146338CAA496F0A"
RP="Points_10_9533B9104470D8E053E7ACA5C4C9F865"

def cls(name): return "/Script/LaserRifleMod."+name

def _coord(x,y): return "(%s=%d,%s=%d)"%(CX,x,CY,y)
def _parents(ps): return "" if not ps else "("+",".join(_coord(px,py) for (px,py) in ps)+")"
def _children(edges):
    if not edges: return "()"
    parts=[]
    for (cc,road) in edges:
        pts=",".join(_coord(px,py) for (px,py) in road)
        parts.append("(%s, (%s=(%s)))"%(_coord(cc[0],cc[1]),RP,pts))
    return "("+",".join(parts)+")"
def _carr(cs): return "" if not cs else "("+",".join(_coord(cx,cy) for (cx,cy) in cs)+")"
def _node_struct(sch, col, row, parents, children, unhide, to_unhide):
    s=('"/Script/Engine.BlueprintGeneratedClass\''+sch+'\'"') if sch else "None"
    return ("("+F_SCH+"="+s+","+F_CRD+"="+_coord(col,row)+","+F_PAR+"="+_parents(parents)+","+
            F_CHR+"="+_children(children)+","+F_UNH+"="+_carr(unhide)+","+F_NTU+"="+_carr(to_unhide)+")")

def build_tree(asset_name, display, pre_desc, post_desc, columns):
    """columns = list of columns; each column = ordered list of schematic class
    names laid out vertically with a linear parent/unhide chain."""
    node_bp=unreal.load_class(None, NODE_BP_CLASS_PATH)
    rt=unreal.load_class(None, RT_PARENT_CLASS)
    if not node_bp or not rt: err("cannot load base classes"); return False

    at=unreal.AssetToolsHelpers.get_asset_tools()
    f=unreal.BlueprintFactory(); f.set_editor_property("parent_class", rt)
    try: unreal.EditorAssetLibrary.make_directory(PKG_DIR)
    except Exception: pass
    full=PKG_DIR+"/"+asset_name
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        unreal.EditorAssetLibrary.delete_asset(full)
    bp=at.create_asset(asset_name, PKG_DIR, unreal.Blueprint, f)
    if not bp: err("could not create "+full); return False
    cdo=unreal.get_default_object(bp.generated_class())
    try:
        cdo.set_editor_property("mDisplayName", unreal.Text(display))
        cdo.set_editor_property("mPreUnlockDisplayName", unreal.Text(display))
        cdo.set_editor_property("mPreUnlockDescription", unreal.Text(pre_desc))
        cdo.set_editor_property("mPostUnlockDescription", unreal.Text(post_desc))
    except Exception as e: log("  display warn: "+str(e))

    # MAM left-list row + tree-header card icon = UFGResearchTree::mResearchTreeIcon (FSlateBrush);
    # the script never set it -> blank row + white-square header. Set it from our rifle icon.
    ipath = "/LaserRifleMod/Equipment/LaserRifle/Icons/T_LaserRifle_Icon_Mk%d" % (5 if "Systems" in asset_name else 1)
    tex = unreal.load_asset(ipath)
    if tex:
        try:
            brush = unreal.SlateBrush()
            brush.set_editor_property("resource_object", tex)
            brush.set_editor_property("image_size", unreal.Vector2D(64.0, 64.0))
            cdo.set_editor_property("mResearchTreeIcon", brush)
            log("  tree icon set (obj): "+ipath)
        except Exception as e:
            # fallback: import_text into the existing brush struct if SlateBrush/enum bindings differ
            b = cdo.get_editor_property("mResearchTreeIcon")
            b.import_text("(DrawAs=Image,ImageSize=(X=64,Y=64),ResourceObject=Texture2D'\"%s.%s\"')" % (ipath, ipath.rsplit("/",1)[-1]))
            cdo.set_editor_property("mResearchTreeIcon", b)
            log("  tree icon set (import_text): "+ipath)
    else:
        log("  tree icon MISSING: "+ipath)

    nodes=[]; missing=0
    for col, names in enumerate(columns):
        last=len(names)-1
        for row, name in enumerate(names):
            sch=unreal.load_class(None, cls(name))
            if not sch: log("  MISSING "+name); missing+=1
            spath=sch.get_path_name() if sch else None
            prev=(col,row-1); nxt=(col,row+1)
            parents   =[prev] if row>0 else []
            children  =[(nxt,[(col,row),nxt])] if row<last else []
            unhide    =[prev] if row>0 else []
            to_unhide =[nxt] if row<last else []
            nd_name="Node_%s_%d_%d"%(asset_name,col,row)
            node=unreal.new_object(node_bp, cdo, nd_name)
            txt=_node_struct(spath,col,row,parents,children,unhide,to_unhide)
            try:
                nd=node.get_editor_property("mNodeDataStruct"); nd.import_text(txt)
                node.set_editor_property("mNodeDataStruct", nd)
            except Exception as e: err("  "+nd_name+" import FAILED: "+str(e))
            nodes.append(node)
    cdo.set_editor_property("mNodes", nodes)
    try: unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    except Exception as e: log("  compile warn: "+str(e))
    unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
    log("  %s: %d nodes (%d missing)"%(asset_name, len(nodes), missing))
    return missing==0

def main():
    log("====== create_mam_tree START ======")
    # Tree 1: Mk + damage-line chain (slice: damage tiers only on the Mk1 line).
    mk = ["Schematic_LaserRifle_%02d"%n for n in range(1,11)]
    dmg = ["Schematic_LR_Dmg_%02d"%n for n in range(1,6)]
    main_col = [mk[0]] + dmg + mk[1:]          # Mk1, Dmg I..V, Mk2..Mk10
    ok1 = build_tree("MAM_LaserRifle", "Laser Rifle",
        "Research the craftable laser rifle and its Mk + damage upgrades.",
        "Each damage tier raises damage; finishing a Mk's damage line unlocks the next Mk.",
        [main_col])
    # Tree 2: systems -- Heat Capacity column + Cooling column.
    heat = ["Schematic_LR_Heat_%02d"%n for n in range(1,3)]
    cool = ["Schematic_LR_Cool_%02d"%n for n in range(1,3)]
    ok2 = build_tree("MAM_LaserRifle_Systems", "Laser Rifle - Systems",
        "Upgrade the laser rifle's heat handling.",
        "Heat Capacity adds shots before overheat; Cooling speeds recharge.",
        [heat, cool])
    log("====== DONE main=%s systems=%s ======"%(ok1, ok2))
    return ok1 and ok2

try:
    ok=main(); log("Script "+("SUCCEEDED" if ok else "FAILED")); sys.exit(0 if ok else 1)
except Exception as e:
    err("Unhandled: "+str(e)); traceback.print_exc(); sys.exit(1)
