# import_kit_probe2.py -- fix pass 2: reimport the ORIENTATION-CORRECTED composed Mk1
# kit rifle (barrel now on local +X) into the proven Meshes_Tripo/Rifle_Mk01 slot, and
# build REAL per-part materials (reusing import_tripo.py's proven material-graph pattern,
# once per source part) instead of relying on FBX auto-material-import, which silently
# left every slot's material_interface NULL last time. Run (game/editor/build closed):
#   .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\ue\import_kit_probe2.py
import unreal, os
EQ = "/LaserRifleMod/Equipment/LaserRifle"
MESH_DIR = EQ + "/Meshes_Tripo"
SRC_FBX = r"C:\Claude\Projects\SatisfactoryLaserRifleMod\tripo_rifles\component_library\kit_composed\fbx\Kit_body_mk1_junk_FIXED2.fbx"
TEXROOT = r"C:\Claude\Projects\SatisfactoryLaserRifleMod\tripo_rifles\component_library\kit_composed\_matfix"
PARENT_BP = EQ + "/BP_Equip_LaserRifle"
MK1_CHILD_BP = EQ + "/BP_Equip_LaserRifle_Mk1"
TARGET_CM = 110.0
NAME = "Rifle_Mk01"

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary

def log(m): unreal.log_warning("LR_KITPROBE2 | " + str(m))

# slot material name (as it lands in the FBX) -> source part id (matches _matfix/<id>/)
SLOT_TO_PART = {
    "ConnectorMat": None,  # flat-color, no texture set
    "tripo_node_68d15819-8b8e-40ae-b241-ec65eb0e69c1_material": "ant_stub_coil",
    "tripo_node_3875bb1d-1942-406c-a71f-2b060e2a53a0_material": "batt_canister_taped",
    "tripo_node_97e05dea-64fb-489c-b3cd-8f2a2b53d0de_material": "gauge_digital_strip",
    "tripo_node_3dff7fa7-c34b-4a7b-92c6-8be239d68d01_material": "vent_intake_sleek",
    "tripo_node_3309ab8f-141d-4e49-99aa-da538f09528a_material": "vent_louver_rust",
    "tripo_node_2a1d81c6-8b33-42ee-b947-50d4780b7dac_material": "wire_sheathed",
    "tripo_node_49fff4b3-c25f-471b-a844-8086a1685d45_material_001": "body_mk1_junk",
    "tripo_node_49fff4b3-c25f-471b-a844-8086a1685d45_material.001": "body_mk1_junk",
}

def fbx_opts(scale):
    o = unreal.FbxImportUI()
    o.import_mesh = True
    o.import_as_skeletal = False
    o.import_materials = False   # we build materials explicitly this time (proven pattern)
    o.import_textures = False
    o.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    sm = o.static_mesh_import_data
    sm.set_editor_property("combine_meshes", True)
    sm.set_editor_property("auto_generate_collision", True)
    sm.set_editor_property("import_uniform_scale", float(scale))
    return o

def import_mesh(fbx, name, scale):
    if eal.does_asset_exist(MESH_DIR + "/" + name):
        eal.delete_asset(MESH_DIR + "/" + name)
    t = unreal.AssetImportTask()
    t.filename = fbx
    t.destination_path = MESH_DIR
    t.destination_name = name
    t.replace_existing = True
    t.automated = True
    t.save = True
    t.options = fbx_opts(scale)
    tools.import_asset_tasks([t])
    return MESH_DIR + "/" + name

def longest(path):
    m = eal.load_asset(path)
    if not m: return 0.0
    box = m.get_bounding_box()
    mn = box.get_editor_property("min"); mx = box.get_editor_property("max")
    e = mx - mn
    return max(e.x, e.y, e.z)

def import_tex(png, dest_dir, name, srgb, normal=False):
    if not os.path.exists(png):
        return None
    t = unreal.AssetImportTask()
    t.filename = png
    t.destination_path = dest_dir
    t.destination_name = name
    t.replace_existing = True
    t.automated = True
    t.save = True
    tools.import_asset_tasks([t])
    tex = eal.load_asset(dest_dir + "/" + name)
    if tex:
        tex.set_editor_property("srgb", srgb)
        if normal:
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property("flip_green_channel", True)
        eal.save_asset(dest_dir + "/" + name)
    return tex

def build_material(part_id, base, normal, rough, metal):
    mpath = MESH_DIR + "/M_Kit_" + part_id
    if eal.does_asset_exist(mpath): eal.delete_asset(mpath)
    mat = tools.create_asset("M_Kit_" + part_id, MESH_DIR, unreal.Material, unreal.MaterialFactoryNew())
    def samp(tex, x, y, stype):
        n = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSample, x, y)
        n.texture = tex
        n.set_editor_property("sampler_type", stype)
        return n
    if base:
        b = samp(base, -300, -150, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        MEL.connect_material_property(b, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    if metal:
        mt = samp(metal, -300, 40, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        MEL.connect_material_property(mt, "R", unreal.MaterialProperty.MP_METALLIC)
    if rough:
        r = samp(rough, -300, 220, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        MEL.connect_material_property(r, "R", unreal.MaterialProperty.MP_ROUGHNESS)
    if normal:
        nm = samp(normal, -300, 400, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        MEL.connect_material_property(nm, "RGB", unreal.MaterialProperty.MP_NORMAL)
    MEL.recompile_material(mat)
    eal.save_asset(mpath)
    return mat

def build_flat_material(name, color):
    mpath = MESH_DIR + "/M_Kit_" + name
    if eal.does_asset_exist(mpath): eal.delete_asset(mpath)
    mat = tools.create_asset("M_Kit_" + name, MESH_DIR, unreal.Material, unreal.MaterialFactoryNew())
    c = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -300, -150)
    c.set_editor_property("constant", unreal.LinearColor(*color))
    MEL.connect_material_property(c, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.recompile_material(mat)
    eal.save_asset(mpath)
    return mat

if not os.path.exists(SRC_FBX):
    log("BLOCKER: source fbx not found: " + SRC_FBX)
else:
    import_mesh(SRC_FBX, NAME, 1.0)
    l1 = longest(MESH_DIR + "/" + NAME)
    sc = round(TARGET_CM / l1, 4) if l1 > 0 else 1.0
    import_mesh(SRC_FBX, NAME, sc)
    mesh_path = MESH_DIR + "/" + NAME
    mesh = eal.load_asset(mesh_path)

    if not mesh:
        log("BLOCKER: mesh import failed")
    else:
        box = mesh.get_bounding_box()
        mn = box.get_editor_property("min"); mx = box.get_editor_property("max")
        ext = mx - mn
        log("Imported %s scale=%s final_bbox=(%.1f,%.1f,%.1f) longest=%.1fcm (longestIsX=%s)" % (
            NAME, sc, ext.x, ext.y, ext.z, max(ext.x, ext.y, ext.z), ext.x >= max(ext.y, ext.z)))

        slots = mesh.get_editor_property("static_materials")
        log("Slot count: %d" % len(slots))

        # build (or reuse) a material per unique source part
        built = {}
        connector_mat = build_flat_material("ConnectorMat", (0.15, 0.35, 0.55, 1.0))

        new_slots = []
        for i, s in enumerate(slots):
            slot_name = str(s.get_editor_property("material_slot_name"))
            part_id = SLOT_TO_PART.get(slot_name, "UNKNOWN")
            if part_id is None:
                mat = connector_mat
            elif part_id == "UNKNOWN":
                log("  slot[%d]=%s -> NO MAPPING FOUND, leaving unassigned" % (i, slot_name))
                mat = None
            else:
                if part_id not in built:
                    texdir = os.path.join(TEXROOT, part_id)
                    tdest = MESH_DIR + "/Kit_Tex_" + part_id
                    if not eal.does_directory_exist(tdest): eal.make_directory(tdest)
                    base = import_tex(os.path.join(texdir, "Base Color.png"), tdest, "T_Kit_%s_Base" % part_id, True)
                    normal = import_tex(os.path.join(texdir, "Normal.png"), tdest, "T_Kit_%s_Normal" % part_id, False, True)
                    rough = import_tex(os.path.join(texdir, "Roughness.png"), tdest, "T_Kit_%s_Rough" % part_id, False)
                    metal = import_tex(os.path.join(texdir, "Metallic.png"), tdest, "T_Kit_%s_Metal" % part_id, False)
                    built[part_id] = build_material(part_id, base, normal, rough, metal)
                    log("  built material for part=%s (base=%s norm=%s rough=%s metal=%s)" % (
                        part_id, bool(base), bool(normal), bool(rough), bool(metal)))
                mat = built[part_id]
            new = unreal.StaticMaterial()
            new.set_editor_property("material_interface", mat)
            new.set_editor_property("material_slot_name", s.get_editor_property("material_slot_name"))
            new_slots.append(new)
            log("  slot[%d]=%s -> part=%s -> mat=%s" % (i, slot_name, part_id, mat.get_name() if mat else "NONE"))

        mesh.set_editor_property("static_materials", new_slots)
        eal.save_asset(mesh_path)

        # re-verify slots actually stuck
        chk = eal.load_asset(mesh_path).get_editor_property("static_materials")
        for i, s in enumerate(chk):
            m = s.get_editor_property("material_interface")
            log("  VERIFY slot[%d] material_interface = %s" % (i, m.get_name() if m else "NONE"))

        def resync_cdo(bp_path, label):
            bp = eal.load_asset(bp_path)
            if not bp:
                log("BLOCKER: BP not found " + bp_path); return
            cdo = unreal.get_default_object(bp.generated_class())
            cur = list(cdo.get_editor_property("LevelBodyMeshes"))
            cur[0] = mesh
            cdo.set_editor_property("LevelBodyMeshes", cur)
            cdo.set_editor_property("BodyMaterial", None)
            try:
                unreal.BlueprintEditorLibrary.compile_blueprint(bp)
            except Exception as e:
                log("%s compile warn: %s" % (label, str(e)))
            unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
            log("%s: LevelBodyMeshes[0] re-set to %s, saved." % (label, NAME))

        resync_cdo(PARENT_BP, "parent BP_Equip_LaserRifle")
        resync_cdo(MK1_CHILD_BP, "child BP_Equip_LaserRifle_Mk1")

        log("DONE. Rebuild to cook (bump C++ marker first).")
