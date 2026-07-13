# import_kit_component_pool.py -- headless import of the 30 standalone "kit component" static
# meshes for the procedural kit-component pool feature. Modeled on import_kit_probe2.py's proven
# pattern: two-pass scale-normalize import + explicit material-graph build from separate
# Base Color/Normal/Roughness/Metallic PNGs (FBX auto-import-materials is broken in this project --
# every slot ends up NULL). Unlike probe2 (8 slots on one combined rifle), this is one mesh + one
# material PER PART, which is simpler.
#
# Run (game/editor/build closed):
#   .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\ue\import_kit_component_pool.py
import unreal, os

EQ = "/LaserRifleMod/Equipment/LaserRifle"
KIT_DIR = EQ + "/Components/Kit"
FBX_ROOT = r"C:\Claude\Projects\SatisfactoryLaserRifleMod\tripo_rifles\component_library\kit_parts\fbx"
TEXROOT = r"C:\Claude\Projects\SatisfactoryLaserRifleMod\tripo_rifles\component_library\kit_parts\_matfix"
PARENT_BP = EQ + "/BP_Equip_LaserRifle"
TARGET_CM = 20.0  # reference "unit" longest dimension for a standalone part; C++ rescales per-body at runtime

IDS = [
    "batt_car_scrap", "batt_canister_taped", "batt_cell_pack", "batt_cell_sleek",
    "ant_bent_rod", "ant_stub_coil", "ant_folding", "ant_emitter_spike",
    "gauge_analog_dial", "gauge_dual_dial", "gauge_digital_strip",
    "vent_louver_rust", "vent_fan_exposed", "vent_fan_shrouded", "vent_intake_sleek",
    "wire_loose_bundle", "wire_taped_run", "wire_sheathed",
    "tape_collar_grey", "tape_collar_red",
    "cap_can_leaky", "cap_bank_mid", "cap_cell_sleek",
    "scope_makeshift", "scope_red_dot", "scope_holo_sleek",
    "light_taped_flashlight", "light_strip_led", "light_orb_sleek",
    "core_energy_crystal",
]
assert len(IDS) == 30, len(IDS)

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary


def log(m):
    unreal.log_warning("LR_KITPOOL | " + str(m))


# ---- proven functions, reused near-verbatim from import_kit_probe2.py ----

def fbx_opts(scale):
    o = unreal.FbxImportUI()
    o.import_mesh = True
    o.import_as_skeletal = False
    o.import_materials = False   # we build materials explicitly (proven pattern -- FBX auto-import leaves slots NULL)
    o.import_textures = False
    o.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    sm = o.static_mesh_import_data
    sm.set_editor_property("combine_meshes", True)
    sm.set_editor_property("auto_generate_collision", True)
    sm.set_editor_property("import_uniform_scale", float(scale))
    return o


def import_mesh(fbx, dest_dir, name, scale):
    dest_path = dest_dir + "/" + name
    if eal.does_asset_exist(dest_path):
        eal.delete_asset(dest_path)
    t = unreal.AssetImportTask()
    t.filename = fbx
    t.destination_path = dest_dir
    t.destination_name = name
    t.replace_existing = True
    t.automated = True
    t.save = True
    t.options = fbx_opts(scale)
    tools.import_asset_tasks([t])
    return dest_path


def longest(path):
    m = eal.load_asset(path)
    if not m:
        return 0.0
    box = m.get_bounding_box()
    mn = box.get_editor_property("min")
    mx = box.get_editor_property("max")
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


def build_material(part_id, dest_dir, base, normal, rough, metal):
    mpath = dest_dir + "/M_Kit_" + part_id
    if eal.does_asset_exist(mpath):
        eal.delete_asset(mpath)
    mat = tools.create_asset("M_Kit_" + part_id, dest_dir, unreal.Material, unreal.MaterialFactoryNew())

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


# ---- main import loop, per-part, incremental logging, never abort on a single failure ----

if not eal.does_directory_exist(KIT_DIR):
    eal.make_directory(KIT_DIR)

imported_meshes = {}   # id -> mesh asset path (only successes)
tex_report = {}         # id -> (base, normal, rough, metal) booleans
failures = []

for pid in IDS:
    try:
        fbx = os.path.join(FBX_ROOT, "SM_Kit_%s.fbx" % pid)
        if not os.path.exists(fbx):
            log("FAIL %s: source fbx not found: %s" % (pid, fbx))
            failures.append((pid, "missing fbx"))
            continue

        mesh_name = "SM_Kit_" + pid

        # pass 1: import at scale 1.0 to measure
        import_mesh(fbx, KIT_DIR, mesh_name, 1.0)
        l1 = longest(KIT_DIR + "/" + mesh_name)
        if l1 <= 0:
            log("FAIL %s: degenerate bounding box after pass 1 (longest=%.4f)" % (pid, l1))
            failures.append((pid, "degenerate bbox"))
            continue

        # pass 2: reimport at the scale that normalizes longest dim to TARGET_CM
        sc = round(TARGET_CM / l1, 4)
        import_mesh(fbx, KIT_DIR, mesh_name, sc)
        mesh_path = KIT_DIR + "/" + mesh_name
        mesh = eal.load_asset(mesh_path)
        if not mesh:
            log("FAIL %s: mesh reload failed after pass 2" % pid)
            failures.append((pid, "reload failed"))
            continue

        box = mesh.get_bounding_box()
        mn = box.get_editor_property("min")
        mx = box.get_editor_property("max")
        ext = mx - mn
        log("%s: imported scale=%s final_bbox=(%.1f,%.1f,%.1f) longest=%.1fcm" % (
            pid, sc, ext.x, ext.y, ext.z, max(ext.x, ext.y, ext.z)))

        # textures + material
        texdir = os.path.join(TEXROOT, pid)
        tdest = KIT_DIR + "/Tex/" + pid
        if not eal.does_directory_exist(tdest):
            eal.make_directory(tdest)

        base = import_tex(os.path.join(texdir, "Base Color.png"), tdest, "T_Kit_%s_Base" % pid, True)
        normal = import_tex(os.path.join(texdir, "Normal.png"), tdest, "T_Kit_%s_Normal" % pid, False, True)
        rough = import_tex(os.path.join(texdir, "Roughness.png"), tdest, "T_Kit_%s_Rough" % pid, False)
        metal = import_tex(os.path.join(texdir, "Metallic.png"), tdest, "T_Kit_%s_Metal" % pid, False)
        tex_report[pid] = (bool(base), bool(normal), bool(rough), bool(metal))

        if not base:
            log("WARN %s: no Base Color texture found -- material will have no base color input" % pid)

        mat = build_material(pid, KIT_DIR, base, normal, rough, metal)

        # assign to slot 0
        slots = mesh.get_editor_property("static_materials")
        if len(slots) == 0:
            log("FAIL %s: mesh has zero material slots" % pid)
            failures.append((pid, "no material slots"))
            continue
        new_slots = []
        for i, s in enumerate(slots):
            new = unreal.StaticMaterial()
            new.set_editor_property("material_interface", mat if i == 0 else s.get_editor_property("material_interface"))
            new.set_editor_property("material_slot_name", s.get_editor_property("material_slot_name"))
            new_slots.append(new)
        mesh.set_editor_property("static_materials", new_slots)
        eal.save_asset(mesh_path)

        # verify slot 0 stuck
        chk = eal.load_asset(mesh_path).get_editor_property("static_materials")
        m0 = chk[0].get_editor_property("material_interface")
        if not m0:
            log("FAIL %s: slot 0 material_interface is NULL after save" % pid)
            failures.append((pid, "material assign failed"))
            continue

        imported_meshes[pid] = mesh_path
        log("%s done, mesh=%s" % (pid, mesh_path))

    except Exception as e:
        log("FAIL %s: exception: %s" % (pid, str(e)))
        failures.append((pid, "exception: %s" % str(e)))
        continue

# ---- summary ----
log("=== SUMMARY: %d/%d parts imported successfully ===" % (len(imported_meshes), len(IDS)))
full_mat = [pid for pid, t in tex_report.items() if all(t)]
partial_mat = [pid for pid, t in tex_report.items() if any(t) and not all(t)]
log("full PBR (4/4 maps): %d -> %s" % (len(full_mat), full_mat))
if partial_mat:
    log("partial PBR maps: %d -> %s" % (len(partial_mat), partial_mat))
if failures:
    log("FAILURES: %d -> %s" % (len(failures), failures))

# ---- populate KitComponentPool on the BP CDO, IF that property already exists ----
bp = eal.load_asset(PARENT_BP)
if not bp:
    log("BLOCKER: parent BP not found at %s -- cannot check/populate KitComponentPool" % PARENT_BP)
else:
    cdo = unreal.get_default_object(bp.generated_class())
    try:
        # touch the property first; raises if it doesn't exist on this CDO
        _ = cdo.get_editor_property("KitComponentPool")
        has_prop = True
    except Exception:
        has_prop = False

    if not has_prop:
        log("KitComponentPool property not found on BP yet -- C++ probably not built yet, "
            "skipping CDO populate, meshes are still fully imported and ready")
    else:
        try:
            mesh_assets = [eal.load_asset(p) for p in imported_meshes.values()]
            mesh_assets = [m for m in mesh_assets if m]
            cdo.set_editor_property("KitComponentPool", mesh_assets)
            try:
                unreal.BlueprintEditorLibrary.compile_blueprint(bp)
            except Exception as e:
                log("BP compile warn: %s" % str(e))
            unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
            log("KitComponentPool populated with %d mesh references and saved." % len(mesh_assets))
        except Exception as e:
            log("FAIL populating KitComponentPool despite property existing: %s" % str(e))

log("DONE.")
