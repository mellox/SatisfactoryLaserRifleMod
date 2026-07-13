r"""
import_kit_pool_skeletal.py -- headless import of the 30 kit parts as SKELETAL meshes
(SK_Kit_<id>), replacing the static SM_Kit_* pool per the user's skeletons direction.
Each part was rigged in Blender with: root bone (rigid skin, whole mesh) + conn_0
[+ conn_1 for elongated parts] anchor bones marking where connector wires plug in.

Materials: reuses the ALREADY-BUILT M_Kit_<id> materials from the static pool import
(import_kit_component_pool.py) -- FBX auto-import materials are a known-broken pattern
here, and the textures/material graphs don't change with the rigging.

Run (game/editor/build closed):
  .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\ue\import_kit_pool_skeletal.py
"""
import unreal, os

EQ = "/LaserRifleMod/Equipment/LaserRifle"
KIT_DIR = EQ + "/Components/Kit"
FBX_ROOT = r"C:\Claude\Projects\SatisfactoryLaserRifleMod\tripo_rifles\component_library\kit_parts\fbx_rigged"
TARGET_CM = 20.0  # same normalization as the static pool; C++ rescales per-body at runtime

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


def log(m):
    unreal.log_warning("LR_SKPOOL | " + str(m))


def fbx_opts(scale):
    o = unreal.FbxImportUI()
    o.import_mesh = True
    o.import_as_skeletal = True
    o.import_materials = False
    o.import_textures = False
    o.import_animations = False
    o.create_physics_asset = False   # rigid parts, bones are anchors only
    o.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    sk = o.skeletal_mesh_import_data
    sk.set_editor_property("import_morph_targets", False)
    sk.set_editor_property("use_t0_as_ref_pose", True)
    sk.set_editor_property("import_uniform_scale", float(scale))
    sk.set_editor_property("convert_scene", True)
    sk.set_editor_property("normal_import_method",
                           unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)
    return o


def import_sk(fbx, name, scale):
    t = unreal.AssetImportTask()
    t.filename = fbx
    t.destination_path = KIT_DIR
    t.destination_name = name
    t.replace_existing = True
    t.automated = True
    t.save = True
    t.options = fbx_opts(scale)
    tools.import_asset_tasks([t])
    return KIT_DIR + "/" + name


def longest_cm(path):
    m = eal.load_asset(path)
    if not m:
        return 0.0
    b = m.get_bounds()
    e = b.box_extent
    return 2.0 * max(e.x, e.y, e.z)


ok, failures = {}, []
for pid in IDS:
    try:
        fbx = os.path.join(FBX_ROOT, "SK_Kit_%s.fbx" % pid)
        if not os.path.exists(fbx):
            failures.append((pid, "missing fbx")); log("FAIL %s missing fbx" % pid); continue
        name = "SK_Kit_" + pid
        path = import_sk(fbx, name, 1.0)
        l1 = longest_cm(path)
        if l1 <= 0:
            failures.append((pid, "degenerate")); log("FAIL %s degenerate" % pid); continue
        sc = round(TARGET_CM / l1, 4)
        import_sk(fbx, name, sc)
        sk = eal.load_asset(path)
        if not sk or not sk.get_editor_property("skeleton"):
            failures.append((pid, "no skeleton")); log("FAIL %s no skeleton" % pid); continue

        # reuse the proven M_Kit_<pid> material on slot 0 (skeletal materials list)
        mat = eal.load_asset(KIT_DIR + "/M_Kit_" + pid)
        if mat:
            mats = sk.get_editor_property("materials")
            new_mats = []
            for i, s in enumerate(mats):
                nm = unreal.SkeletalMaterial()
                nm.set_editor_property("material_interface",
                                       mat if i == 0 else s.get_editor_property("material_interface"))
                nm.set_editor_property("material_slot_name",
                                       s.get_editor_property("material_slot_name"))
                new_mats.append(nm)
            sk.set_editor_property("materials", new_mats)
            eal.save_asset(path)
        else:
            log("WARN %s: M_Kit material missing, part will be gray" % pid)

        l2 = longest_cm(path)
        log("%s OK longest=%.1fcm mat=%s" % (pid, l2, bool(mat)))
        ok[pid] = path
    except Exception as e:
        failures.append((pid, str(e))); log("FAIL %s exception %s" % (pid, e))

log("=== SUMMARY %d/%d imported ===" % (len(ok), len(IDS)))
if failures:
    log("FAILURES: %s" % failures)

# save the WHOLE directory: skeletal imports auto-create _Skeleton assets that t.save does
# not persist; an unsaved skeleton cooks away -> runtime crash (sf-skeletal-mesh-import-crash).
saved = eal.save_directory(KIT_DIR, only_if_is_dirty=False, recursive=True)
log("save_directory(%s) ok=%s" % (KIT_DIR, saved))
log("DONE.")
