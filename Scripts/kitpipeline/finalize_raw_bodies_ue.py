r"""
finalize_raw_bodies_ue.py -- UE-side finalize for the picked RAW energy-rifle bodies.
Reads each _rawhandoff_<name>.json (written by finalize_raw_bodies_blender.py), imports the
body-only FBX as a static mesh (two-pass scale-normalize to 110cm), builds an EXPLICIT PBR
material graph from the pre-extracted PNGs (NOT FBX auto-import -- that leaves slots NULL in
this project), and wires the mesh + front-slice muzzle offset into LevelBodyMeshes[mk-1] /
LevelMuzzleOffsets[mk-1] on the parent BP and the Mk<mk> child BP CDO.

Textures MUST be pre-extracted with SYSTEM python before running this (glb_extract.py) --
never subprocess sys.executable from UE python (spawns a 2nd editor that deadlocks).

Run headless (game/build closed):
  .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\kitpipeline\finalize_raw_bodies_ue.py
"""
import unreal, os, json, glob

EQ = "/LaserRifleMod/Equipment/LaserRifle"
MESH_DIR = EQ + "/Meshes_Energy"
PARENT_BP = EQ + "/BP_Equip_LaserRifle"
TARGET_CM = 110.0
# Per-Mk HELD-orientation correction (pitch,yaw,roll deg; +pitch=muzzle up), baked into LevelFineRot
# so the sculpted barrel points at the crosshair. The _align long-axis isn't the visual barrel on
# some bodies -> a per-Mk pitch is needed. CONFIRMED in-game 2026-07-05 via lr.HoldPitch. Re-applied
# every finalize so a make_mk_items rebuild (which recreates the child BPs) can't silently drop them.
# Mk8/Mk9 updated from the user's 2026-07-05 in-game lr.HoldPitch/HoldYaw pass (Mk8 10->9, Mk9 added).
FINE_ROT = {6: (3.0, 0.0, 0.0), 7: (2.0, 0.0, 0.0), 8: (9.0, 0.0, 0.0), 9: (1.0, -6.0, 0.0), 10: (5.0, 0.0, 0.0)}
TEXROOT = r"C:\Claude\Projects\SatisfactoryLaserRifleMod\tripo_rifles\mk6to10_v31\_tex"
HANDOFF_GLOB = r"C:\Claude\Projects\SatisfactoryLaserRifleMod\Scripts\kitpipeline\_rawhandoff_*.json"

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary

def log(m): unreal.log_warning("LR_RAWFINAL | " + str(m))

def fbx_opts(scale):
    o = unreal.FbxImportUI()
    o.import_mesh = True; o.import_as_skeletal = False
    o.import_materials = False; o.import_textures = False
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
    b = m.get_bounding_box()
    e = b.get_editor_property("max") - b.get_editor_property("min")
    return max(e.x, e.y, e.z)

def import_tex(png, dest, tname, srgb, normal=False):
    if not os.path.exists(png): return None
    t = unreal.AssetImportTask()
    t.filename = png; t.destination_path = dest; t.destination_name = tname
    t.replace_existing = True; t.automated = True; t.save = True
    tools.import_asset_tasks([t])
    tex = eal.load_asset(dest + "/" + tname)
    if tex:
        tex.set_editor_property("srgb", srgb)
        if normal:
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property("flip_green_channel", True)
        eal.save_asset(dest + "/" + tname)
    return tex

def build_material(body_id, base, normal, rough, metal):
    mpath = MESH_DIR + "/M_Energy_" + body_id
    if eal.does_asset_exist(mpath): eal.delete_asset(mpath)
    mat = tools.create_asset("M_Energy_" + body_id, MESH_DIR, unreal.Material, unreal.MaterialFactoryNew())
    def samp(tex, y, stype):
        n = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -350, y)
        n.texture = tex; n.set_editor_property("sampler_type", stype); return n
    if base:
        b = samp(base, -150, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        MEL.connect_material_property(b, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
        # EMISSIVE: the bright, saturated energy regions (cyan/blue/violet/magenta/gold cores
        # baked into the Tripo base color) self-illuminate in their OWN colour. Emissive =
        # pow(baseColor, GlowSharpness) * EmissiveIntensity -- raising to a power crushes the
        # dark matte metal toward black while the near-white energy cores stay bright, so only
        # the cores glow. Per-Mk colour comes free from each body's texture. EmissiveIntensity
        # is the SAME scalar param the weapon code already drives each equip (no code change).
        pown = MEL.create_material_expression(mat, unreal.MaterialExpressionPower, -80, 620)
        pown.set_editor_property("const_exponent", 4.0)
        MEL.connect_material_expressions(b, "RGB", pown, "Base")
        inten = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -300, 720)
        inten.set_editor_property("parameter_name", "EmissiveIntensity")
        inten.set_editor_property("default_value", 3.0)
        emul = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, 120, 640)
        MEL.connect_material_expressions(pown, "", emul, "A")
        MEL.connect_material_expressions(inten, "", emul, "B")
        MEL.connect_material_property(emul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if metal:
        mt = samp(metal, 40, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        MEL.connect_material_property(mt, "R", unreal.MaterialProperty.MP_METALLIC)
    if rough:
        r = samp(rough, 220, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        MEL.connect_material_property(r, "R", unreal.MaterialProperty.MP_ROUGHNESS)
    if normal:
        nm = samp(normal, 400, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        MEL.connect_material_property(nm, "RGB", unreal.MaterialProperty.MP_NORMAL)
    MEL.recompile_material(mat); eal.save_asset(mpath)
    return mat

if not eal.does_directory_exist(MESH_DIR):
    eal.make_directory(MESH_DIR)

done, fails = [], []
for hf in sorted(glob.glob(HANDOFF_GLOB)):
    data = json.load(open(hf))
    bid = data["body_id"]; mk = int(data["mk_index"]); fbx = data["body_only_fbx"]
    fy, fz = data["muzzle_frac"]
    name = "Energy_Mk%02d" % mk
    try:
        import_mesh(fbx, name, 1.0)
        l1 = longest(MESH_DIR + "/" + name)
        sc = round(TARGET_CM / l1, 4) if l1 > 0 else 1.0
        import_mesh(fbx, name, sc)
        mesh_path = MESH_DIR + "/" + name
        mesh = eal.load_asset(mesh_path)
        if not mesh:
            fails.append((bid, "import failed")); log("FAIL %s import" % bid); continue

        texdir = os.path.join(TEXROOT, bid)
        tdest = MESH_DIR + "/Tex_" + name
        if not eal.does_directory_exist(tdest): eal.make_directory(tdest)
        base = import_tex(os.path.join(texdir, "Base Color.png"), tdest, "T_%s_Base" % name, True)
        normal = import_tex(os.path.join(texdir, "Normal.png"), tdest, "T_%s_Normal" % name, False, True)
        rough = import_tex(os.path.join(texdir, "Roughness.png"), tdest, "T_%s_Rough" % name, False)
        metal = import_tex(os.path.join(texdir, "Metallic.png"), tdest, "T_%s_Metal" % name, False)
        mat = build_material(name, base, normal, rough, metal)

        slots = mesh.get_editor_property("static_materials")
        ns = []
        for i, s in enumerate(slots):
            sm = unreal.StaticMaterial()
            sm.set_editor_property("material_interface", mat if i == 0 else s.get_editor_property("material_interface"))
            sm.set_editor_property("material_slot_name", s.get_editor_property("material_slot_name"))
            ns.append(sm)
        mesh.set_editor_property("static_materials", ns)
        eal.save_asset(mesh_path)

        box = mesh.get_bounding_box()
        mn = box.get_editor_property("min"); mx = box.get_editor_property("max")
        cy = (mn.y + mx.y)/2.0; hy = (mx.y - mn.y)/2.0
        cz = (mn.z + mx.z)/2.0; hz = (mx.z - mn.z)/2.0
        # MUZZLE = the front-5% TIP centroid (Max.X, cy+fy*hy, cz+fz*hz from the handoff) = the
        # actual EMITTER, so the beam starts AT the muzzle (user: "not starting the beam where
        # the muzzle is"). Front-CENTER (used one build ago) put the origin mid-body/off the
        # emitter; the old front-10% slice put it ~60% up (angled beam). The 5% tip is on the
        # emitter at a natural barrel height -- beam-at-muzzle without the extreme angle.
        muzzle = unreal.Vector(mx.x, cy + fy*hy, cz + fz*hz)
        # HOLD SCALE (fixes "some are too big"): all bodies normalize to 110cm LENGTH, but the
        # energy rifles are far TALLER/bulkier than the old line (Mk6 is 88cm tall vs old Mk4's
        # 29cm), so at the shared default 0.6 hold they fill the screen. Set a per-tier hold
        # scale that targets a consistent in-hand HEIGHT (~33cm, matching the old bodies' held
        # bulk) computed from THIS body's real z-height. User-tunable via the grip sliders.
        full_z = max(mx.z - mn.z, 1e-3)
        hold = round(max(0.20, min(0.70, 33.0 / full_z)), 3)

        def resync(bp_path, label):
            bp = eal.load_asset(bp_path)
            if not bp:
                log("BLOCKER: BP not found " + bp_path); return
            cdo = unreal.get_default_object(bp.generated_class())
            cur = list(cdo.get_editor_property("LevelBodyMeshes"))
            while len(cur) < 10: cur.append(None)
            cur[mk-1] = mesh
            cdo.set_editor_property("LevelBodyMeshes", cur)
            muz = list(cdo.get_editor_property("LevelMuzzleOffsets"))
            while len(muz) < 10: muz.append(unreal.Vector(0,0,0))
            muz[mk-1] = muzzle
            cdo.set_editor_property("LevelMuzzleOffsets", muz)
            hs = list(cdo.get_editor_property("LevelHoldScales"))
            while len(hs) < 10: hs.append(0.0)
            hs[mk-1] = hold
            cdo.set_editor_property("LevelHoldScales", hs)
            fr = list(cdo.get_editor_property("LevelFineRot"))
            while len(fr) < 10: fr.append(unreal.Rotator(0.0, 0.0, 0.0))
            fp_, fy_, fr_ = FINE_ROT.get(mk, (0.0, 0.0, 0.0))
            fr[mk-1] = unreal.Rotator(roll=fr_, pitch=fp_, yaw=fy_)
            cdo.set_editor_property("LevelFineRot", fr)
            try: unreal.BlueprintEditorLibrary.compile_blueprint(bp)
            except Exception as e: log("%s compile warn: %s" % (label, e))
            unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
            log("%s: LevelBodyMeshes[%d]=%s muzzle=%s hold=%s saved" % (label, mk-1, name, muzzle, hold))

        resync(PARENT_BP, "parent")
        resync(EQ + "/BP_Equip_LaserRifle_Mk%d" % mk, "child Mk%d" % mk)
        done.append(bid)
        log("%s -> Mk%d DONE (mat base=%s)" % (bid, mk, bool(base)))
    except Exception as e:
        fails.append((bid, str(e))); log("FAIL %s: %s" % (bid, e))

log("=== SUMMARY %d/%d ok %s ===" % (len(done), len(done)+len(fails), ("FAILS: %s" % fails) if fails else ""))
eal.save_directory(MESH_DIR, only_if_is_dirty=False, recursive=True)
log("DONE.")
