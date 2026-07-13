# verify_kit_material.py -- READ-ONLY: check whether the auto-imported FBX materials on
# Rifle_Mk01 (the kit-probe swap) actually have Base Color wired to a real texture, or
# fell back to a default/empty material. No edits, no build.
import unreal
EQ = "/LaserRifleMod/Equipment/LaserRifle"
eal = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary

def log(m): unreal.log_warning("LR_MATCHECK | " + str(m))

mesh = eal.load_asset(EQ + "/Meshes_Tripo/Rifle_Mk01")
if not mesh:
    log("BLOCKER: mesh not found")
else:
    slots = mesh.get_editor_property("static_materials")
    log("Slot count: %d" % len(slots))
    for i, s in enumerate(slots):
        mat = s.get_editor_property("material_interface")
        name = mat.get_name() if mat else "NONE"
        info = "slot[%d]=%s" % (i, name)
        if mat and hasattr(mat, "get_editor_property"):
            try:
                base = mat.get_editor_property("base_color") if False else None
            except Exception:
                pass
            # walk the expressions for a texture sample feeding base color
            try:
                exprs = MEL.get_material_property_input_node(mat, unreal.MaterialProperty.MP_BASE_COLOR)
                info += " base_color_input=%s" % (exprs.get_name() if exprs else "UNCONNECTED")
            except Exception as e:
                info += " (could not inspect: %s)" % str(e)
        log(info)
