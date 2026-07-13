# import_kit_bodyonly_mk1.py -- fix pass 3: swap Rifle_Mk01 to the BARE body (no baked-in
# parts) so lr.RandomComponents' dynamically-attached parts are the ONLY parts visible,
# instead of stacking on top of parts already baked into the composed mesh's geometry.
# Also sets LevelMuzzleOffsets[0] from this mesh's own bbox (Max.X convention, same as
# import_tripo.py uses for every other tier) -- the muzzle/beam-origin gap flagged since
# the first kit-probe swap. Run (game/editor/build closed):
#   .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\ue\import_kit_bodyonly_mk1.py
import unreal, os
EQ = "/LaserRifleMod/Equipment/LaserRifle"
MESH_DIR = EQ + "/Meshes_Tripo"
SRC_FBX = r"C:\Claude\Projects\SatisfactoryLaserRifleMod\tripo_rifles\component_library\kit_composed\fbx\Kit_body_mk1_junk_BODYONLY.fbx"
TEXROOT = r"C:\Claude\Projects\SatisfactoryLaserRifleMod\tripo_rifles\component_library\kit_composed\_matfix"
PARENT_BP = EQ + "/BP_Equip_LaserRifle"
MK1_CHILD_BP = EQ + "/BP_Equip_LaserRifle_Mk1"
TARGET_CM = 110.0
NAME = "Rifle_Mk01"
PART_ID = "body_mk1_junk"   # the _matfix texture folder to reuse (already extracted)

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary

def log(m): unreal.log_warning("LR_BODYONLY | " + str(m))

def fbx_opts(scale):
    o = unreal.FbxImportUI()
    o.import_mesh = True
    o.import_as_skeletal = False
    o.import_materials = False
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
    t.filename = fbx; t.destination_path = MESH_DIR; t.destination_name = name
    t.replace_existing = True; t.automated = True; t.save = True
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
    if not os.path.exists(png): return None
    t = unreal.AssetImportTask()
    t.filename = png; t.destination_path = dest_dir; t.destination_name = name
    t.replace_existing = True; t.automated = True; t.save = True
    tools.import_asset_tasks([t])
    tex = eal.load_asset(dest_dir + "/" + name)
    if tex:
        tex.set_editor_property("srgb", srgb)
        if normal:
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property("flip_green_channel", True)
        eal.save_asset(dest_dir + "/" + name)
    return tex

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

        # Single real material (body-only, one slot) -- same proven build pattern.
        texdir = os.path.join(TEXROOT, PART_ID)
        tdest = MESH_DIR + "/Kit_Tex_" + PART_ID
        if not eal.does_directory_exist(tdest): eal.make_directory(tdest)
        base = import_tex(os.path.join(texdir, "Base Color.png"), tdest, "T_Kit_%s_Base" % PART_ID, True)
        normal = import_tex(os.path.join(texdir, "Normal.png"), tdest, "T_Kit_%s_Normal" % PART_ID, False, True)
        rough = import_tex(os.path.join(texdir, "Roughness.png"), tdest, "T_Kit_%s_Rough" % PART_ID, False)
        metal = import_tex(os.path.join(texdir, "Metallic.png"), tdest, "T_Kit_%s_Metal" % PART_ID, False)

        mpath = MESH_DIR + "/M_Kit_" + PART_ID + "_body"
        if eal.does_asset_exist(mpath): eal.delete_asset(mpath)
        mat = tools.create_asset("M_Kit_" + PART_ID + "_body", MESH_DIR, unreal.Material, unreal.MaterialFactoryNew())
        def samp(tex, x, y, stype):
            n = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSample, x, y)
            n.texture = tex; n.set_editor_property("sampler_type", stype)
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

        slots = mesh.get_editor_property("static_materials")
        new_slots = []
        for s in slots:
            new = unreal.StaticMaterial()
            new.set_editor_property("material_interface", mat)
            new.set_editor_property("material_slot_name", s.get_editor_property("material_slot_name"))
            new_slots.append(new)
        mesh.set_editor_property("static_materials", new_slots)
        eal.save_asset(mesh_path)
        log("Material assigned to all %d slot(s): %s" % (len(new_slots), mat.get_name()))

        # Muzzle offset: bbox Max.X, Y/Z centered (matches import_tripo.py's established
        # per-Mk muzzle convention exactly -- this rifle's beam has been mis-origined since
        # the very first kit-probe swap; this is the actual fix).
        muzzle = unreal.Vector(mx.x, (mn.y + mx.y) / 2.0, (mn.z + mx.z) / 2.0)
        log("Computed muzzle offset: (%.1f, %.1f, %.1f)" % (muzzle.x, muzzle.y, muzzle.z))

        def resync_cdo(bp_path, label):
            bp = eal.load_asset(bp_path)
            if not bp:
                log("BLOCKER: BP not found " + bp_path); return
            cdo = unreal.get_default_object(bp.generated_class())
            cur = list(cdo.get_editor_property("LevelBodyMeshes"))
            cur[0] = mesh
            cdo.set_editor_property("LevelBodyMeshes", cur)
            cdo.set_editor_property("BodyMaterial", None)
            muz = list(cdo.get_editor_property("LevelMuzzleOffsets"))
            if len(muz) == 10:
                muz[0] = muzzle
                cdo.set_editor_property("LevelMuzzleOffsets", muz)
            else:
                log("%s: LevelMuzzleOffsets length %d != 10, skipped muzzle set" % (label, len(muz)))
            try:
                unreal.BlueprintEditorLibrary.compile_blueprint(bp)
            except Exception as e:
                log("%s compile warn: %s" % (label, str(e)))
            unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
            log("%s: LevelBodyMeshes[0]=%s, LevelMuzzleOffsets[0]=%s, saved." % (label, NAME, muzzle))

        resync_cdo(PARENT_BP, "parent BP_Equip_LaserRifle")
        resync_cdo(MK1_CHILD_BP, "child BP_Equip_LaserRifle_Mk1")
        log("DONE. Rebuild to cook (bump C++ marker first).")
