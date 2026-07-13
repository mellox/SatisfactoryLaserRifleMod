# finalize_kit_rifle_ue.py -- DETERMINISTIC, reusable UE-side finishing pass for a
# composed kit rifle. Reads a JSON hand-off (written by finalize_kit_rifle_blender.py's
# finalize() call) instead of any hand-typed slot/material mapping -- no per-run guessing.
#
# USAGE: write the hand-off JSON, then run headless:
#   .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\kitpipeline\finalize_kit_rifle_ue.py
# The hand-off JSON path is fixed (see HANDOFF below) so the driver script just needs to
# write it before invoking this.
#
# Hand-off JSON shape:
# {
#   "body_id": "body_mk1_junk",
#   "mk_index": 1,                          <- which Mk tier (1-10), determines CDO slot + child BP
#   "fbx": "C:\\...\\Kit_body_mk1_junk_FINAL.fbx",
#   "slot_to_part": {"ConnectorMat": null, "tripo_node_xxx_material": "ant_stub_coil", ...},
#   "texture_root": "C:\\...\\component_library\\kit_composed\\_matfix",
#   "part_source_glbs": {"ant_stub_coil": "C:\\...\\kit_parts\\raw\\ant_stub_coil_tex.glb", ...}
# }
import unreal, os, json, subprocess, sys

HANDOFF = r"C:\Claude\Projects\SatisfactoryLaserRifleMod\Scripts\kitpipeline\_handoff.json"
EQ = "/LaserRifleMod/Equipment/LaserRifle"
MESH_DIR = EQ + "/Meshes_Tripo"
PARENT_BP = EQ + "/BP_Equip_LaserRifle"
TARGET_CM = 110.0

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary

def log(m): unreal.log_warning("LR_KITFINAL | " + str(m))

if not os.path.exists(HANDOFF):
    log("BLOCKER: no hand-off file at %s -- run the Blender finalize() step first" % HANDOFF)
else:
    with open(HANDOFF) as f:
        data = json.load(f)

    body_id = data["body_id"]
    mk_index = int(data["mk_index"])
    # BODY-ONLY mesh when the handoff provides one ("all rifles kitted"): the runtime
    # RandomComponents system rolls parts onto a bare body -- a full composed mesh would
    # leave baked-in parts that can never be removed (the Mk1 "parts not removed" bug).
    fbx = data.get("body_only_fbx") or data["fbx"]
    slot_to_part = data["slot_to_part"]
    texroot = data.get("texture_root",
        r"C:\Claude\Projects\SatisfactoryLaserRifleMod\tripo_rifles\component_library\kit_composed\_matfix")
    part_sources = data.get("part_source_glbs", {})
    muzzle_frac = data.get("muzzle_frac")   # [fy, fz] from finalize_kit_rifle_blender.py's
                                             # front-slice-centroid fix -- None on older handoffs
    name = "Rifle_Mk%02d" % mk_index

    # -- textures must be extracted BEFORE this script runs (glb_extract.py with SYSTEM
    # python). NEVER subprocess sys.executable from UE python: inside the editor it is
    # UnrealEditor-Cmd.exe itself, which spawns a SECOND full editor that deadlocks against
    # this one (2026-07-03: the all-rifles batch hung 4+ hours on exactly that). Missing
    # textures are a warn-and-skip now -- the part's material just comes out empty.
    for part_id in part_sources:
        if not os.path.exists(os.path.join(texroot, part_id, "Base Color.png")):
            log("WARNING: textures NOT extracted for %s -- run glb_extract.py with system "
                "python first; this part's material will be empty" % part_id)

    # -- two-pass normalize import --
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

    def import_mesh(fbx_path, dest_name, scale):
        if eal.does_asset_exist(MESH_DIR + "/" + dest_name):
            eal.delete_asset(MESH_DIR + "/" + dest_name)
        t = unreal.AssetImportTask()
        t.filename = fbx_path; t.destination_path = MESH_DIR; t.destination_name = dest_name
        t.replace_existing = True; t.automated = True; t.save = True
        t.options = fbx_opts(scale)
        tools.import_asset_tasks([t])
        return MESH_DIR + "/" + dest_name

    def longest(path):
        m = eal.load_asset(path)
        if not m: return 0.0
        box = m.get_bounding_box()
        mn = box.get_editor_property("min"); mx = box.get_editor_property("max")
        e = mx - mn
        return max(e.x, e.y, e.z)

    import_mesh(fbx, name, 1.0)
    l1 = longest(MESH_DIR + "/" + name)
    scale = round(TARGET_CM / l1, 4) if l1 > 0 else 1.0
    import_mesh(fbx, name, scale)
    mesh_path = MESH_DIR + "/" + name
    mesh = eal.load_asset(mesh_path)

    if not mesh:
        log("BLOCKER: mesh import failed")
    else:
        box = mesh.get_bounding_box()
        mn = box.get_editor_property("min"); mx = box.get_editor_property("max")
        ext = mx - mn
        log("Imported %s scale=%s bbox=(%.1f,%.1f,%.1f) longestIsX=%s" % (
            name, scale, ext.x, ext.y, ext.z, ext.x >= max(ext.y, ext.z)))

        def import_tex(png, dest_dir, tname, srgb, normal=False):
            if not os.path.exists(png): return None
            t = unreal.AssetImportTask()
            t.filename = png; t.destination_path = dest_dir; t.destination_name = tname
            t.replace_existing = True; t.automated = True; t.save = True
            tools.import_asset_tasks([t])
            tex = eal.load_asset(dest_dir + "/" + tname)
            if tex:
                tex.set_editor_property("srgb", srgb)
                if normal:
                    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
                    tex.set_editor_property("flip_green_channel", True)
                eal.save_asset(dest_dir + "/" + tname)
            return tex

        def build_material(part_id, base, normal, rough, metal):
            mpath = MESH_DIR + "/M_Kit_" + part_id
            if eal.does_asset_exist(mpath): eal.delete_asset(mpath)
            mat = tools.create_asset("M_Kit_" + part_id, MESH_DIR, unreal.Material, unreal.MaterialFactoryNew())
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
            MEL.recompile_material(mat); eal.save_asset(mpath)
            return mat

        def build_flat_material(mname, color):
            mpath = MESH_DIR + "/M_Kit_" + mname
            if eal.does_asset_exist(mpath): eal.delete_asset(mpath)
            mat = tools.create_asset("M_Kit_" + mname, MESH_DIR, unreal.Material, unreal.MaterialFactoryNew())
            c = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -300, -150)
            c.set_editor_property("constant", unreal.LinearColor(*color))
            MEL.connect_material_property(c, "", unreal.MaterialProperty.MP_BASE_COLOR)
            MEL.recompile_material(mat); eal.save_asset(mpath)
            return mat

        # UE sanitizes FBX material names (Blender's "material.001" -> "material_001"), so
        # look the slot up under a dot/underscore-normalized key as well as the exact one --
        # Mk1's body slot (the only one with a ".001" suffix) missed its mapping otherwise.
        norm_map = {}
        for k, v in slot_to_part.items():
            norm_map[k] = v
            norm_map[k.replace(".", "_")] = v

        slots = mesh.get_editor_property("static_materials")
        built = {}
        connector_mat = build_flat_material("ConnectorMat", (0.15, 0.35, 0.55, 1.0))
        new_slots = []
        for i, s in enumerate(slots):
            slot_name = str(s.get_editor_property("material_slot_name"))
            part_id = norm_map.get(slot_name, "UNKNOWN")
            if part_id is None:
                mat = connector_mat
            elif part_id == "UNKNOWN":
                log("  slot[%d]=%s -> NO MAPPING (unexpected, check hand-off), leaving unassigned" % (i, slot_name))
                mat = None
            else:
                if part_id not in built:
                    texdir = os.path.join(texroot, part_id)
                    tdest = MESH_DIR + "/Kit_Tex_" + part_id
                    if not eal.does_directory_exist(tdest): eal.make_directory(tdest)
                    base = import_tex(os.path.join(texdir, "Base Color.png"), tdest, "T_Kit_%s_Base" % part_id, True)
                    normal = import_tex(os.path.join(texdir, "Normal.png"), tdest, "T_Kit_%s_Normal" % part_id, False, True)
                    rough = import_tex(os.path.join(texdir, "Roughness.png"), tdest, "T_Kit_%s_Rough" % part_id, False)
                    metal = import_tex(os.path.join(texdir, "Metallic.png"), tdest, "T_Kit_%s_Metal" % part_id, False)
                    built[part_id] = build_material(part_id, base, normal, rough, metal)
                mat = built[part_id]
            ns = unreal.StaticMaterial()
            ns.set_editor_property("material_interface", mat)
            ns.set_editor_property("material_slot_name", s.get_editor_property("material_slot_name"))
            new_slots.append(ns)
        mesh.set_editor_property("static_materials", new_slots)
        eal.save_asset(mesh_path)

        chk = eal.load_asset(mesh_path).get_editor_property("static_materials")
        bad = [i for i, s in enumerate(chk) if not s.get_editor_property("material_interface")]
        if bad:
            log("WARNING: slots with no material after build: %s" % bad)
        else:
            log("All %d material slots verified non-NONE." % len(chk))

        # Muzzle offset: bbox Max.X (unchanged convention) + a FRONT-SLICE-CENTROID fraction
        # for Y/Z (fy, fz), applied to THIS mesh's own real bbox -- not an absolute value, so
        # it's safe across Blender-meters vs UE-cm unit differences. Same fix proven on Mk1
        # ("beam under the barrel" -- whole-bbox-center is wrong for an asymmetric silhouette).
        muzzle = None
        if muzzle_frac:
            fy, fz = muzzle_frac
            cy = (mn.y + mx.y) / 2.0; hy = (mx.y - mn.y) / 2.0
            cz = (mn.z + mx.z) / 2.0; hz = (mx.z - mn.z) / 2.0
            muzzle = unreal.Vector(mx.x, cy + fy * hy, cz + fz * hz)
            log("Computed muzzle offset from muzzle_frac=%s: %s" % (muzzle_frac, muzzle))
        else:
            log("WARNING: no muzzle_frac in hand-off (older finalize() run?) -- "
                "LevelMuzzleOffsets[%d] left untouched." % (mk_index - 1))

        def resync_cdo(bp_path, label):
            bp = eal.load_asset(bp_path)
            if not bp:
                log("BLOCKER: BP not found " + bp_path); return
            cdo = unreal.get_default_object(bp.generated_class())
            cur = list(cdo.get_editor_property("LevelBodyMeshes"))
            cur[mk_index - 1] = mesh
            cdo.set_editor_property("LevelBodyMeshes", cur)
            if muzzle:
                muz = list(cdo.get_editor_property("LevelMuzzleOffsets"))
                if len(muz) == 10:
                    muz[mk_index - 1] = muzzle
                    cdo.set_editor_property("LevelMuzzleOffsets", muz)
                else:
                    log("%s: LevelMuzzleOffsets length %d != 10, skipped" % (label, len(muz)))
            try:
                unreal.BlueprintEditorLibrary.compile_blueprint(bp)
            except Exception as e:
                log("%s compile warn: %s" % (label, str(e)))
            unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
            log("%s: LevelBodyMeshes[%d] -> %s, muzzle=%s, saved." % (
                label, mk_index - 1, name, muzzle))

        resync_cdo(PARENT_BP, "parent")
        resync_cdo(EQ + "/BP_Equip_LaserRifle_Mk%d" % mk_index, "child Mk%d" % mk_index)

        log("DONE body_id=%s mk=%d. Next: bump marker + build.ps1." % (body_id, mk_index))
