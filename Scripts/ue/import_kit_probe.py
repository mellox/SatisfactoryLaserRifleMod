# import_kit_probe.py -- ONE-OFF alignment/scale probe: import a single composed
# component-kit rifle (Kit_body_mk1_junk.fbx) as a STATIC mesh using its OWN embedded
# FBX materials (no custom material graph), two-pass normalize to the proven 110cm
# convention, and swap it into the Mk1 slot on BOTH the parent BP_Equip_LaserRifle
# AND the actual shipped Mk1 child item (BP_Equip_LaserRifle_Mk1) so it's visible
# in-game on the real craftable Mk1 rifle. Only index 0 (Mk1) is touched; Mk2-10
# untouched. Run (game/editor/build closed):
#   .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\ue\import_kit_probe.py
import unreal, os
ROOT = "/LaserRifleMod"
EQ = ROOT + "/Equipment/LaserRifle"
# IMPORTANT: reuse the EXISTING, already-cooked Meshes_Tripo/Rifle_Mk01 path (same as
# import_tripo.py) rather than a brand-new directory -- a prior attempt used a new
# Meshes_KitTest folder and the cooker never discovered it (asset never appeared
# anywhere in the cook log despite saving correctly). Overwriting a known/proven
# already-in-the-cook-manifest asset name sidesteps that gap entirely.
MESH_DIR = EQ + "/Meshes_Tripo"
SRC_FBX = r"C:\Claude\Projects\SatisfactoryLaserRifleMod\tripo_rifles\component_library\kit_composed\fbx\Kit_body_mk1_junk.fbx"
PARENT_BP = EQ + "/BP_Equip_LaserRifle"
MK1_CHILD_BP = EQ + "/BP_Equip_LaserRifle_Mk1"
TARGET_CM = 110.0
NAME = "Rifle_Mk01"

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

def log(m): unreal.log_warning("LR_KITPROBE | " + str(m))

if not eal.does_directory_exist(MESH_DIR):
    eal.make_directory(MESH_DIR)

def fbx_opts(scale):
    o = unreal.FbxImportUI()
    o.import_mesh = True
    o.import_as_skeletal = False
    o.import_materials = True   # let UE parse the FBX's own embedded materials/textures
    o.import_textures = True
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

if not os.path.exists(SRC_FBX):
    log("BLOCKER: source fbx not found: " + SRC_FBX)
else:
    # two-pass normalize (proven pattern from import_tripo.py)
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
        log("Imported %s raw_longest=%.2f scale_applied=%s final_bbox=(%.1f,%.1f,%.1f) final_longest=%.1fcm" % (
            NAME, l1, sc, ext.x, ext.y, ext.z, max(ext.x, ext.y, ext.z)))

        slots = mesh.get_editor_property("static_materials")
        log("Material slots on import: %d (%s)" % (
            len(slots), ", ".join(s.get_editor_property("material_slot_name").__str__() for s in slots)))

        def swap_mk1(bp_path, label):
            bp = eal.load_asset(bp_path)
            if not bp:
                log("BLOCKER: BP not found " + bp_path)
                return
            cdo = unreal.get_default_object(bp.generated_class())
            cur = list(cdo.get_editor_property("LevelBodyMeshes"))
            if len(cur) != 10:
                log("%s: unexpected LevelBodyMeshes length %d, skipping" % (label, len(cur)))
                return
            prev = cur[0].get_name() if cur[0] else "None"
            cur[0] = mesh
            cdo.set_editor_property("LevelBodyMeshes", cur)
            cdo.set_editor_property("BodyMaterial", None)
            try:
                unreal.BlueprintEditorLibrary.compile_blueprint(bp)
            except Exception as e:
                log("%s compile warn: %s" % (label, str(e)))
            unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
            log("%s: LevelBodyMeshes[0] swapped %s -> %s, saved." % (label, prev, NAME))

        swap_mk1(PARENT_BP, "parent BP_Equip_LaserRifle")
        swap_mk1(MK1_CHILD_BP, "child BP_Equip_LaserRifle_Mk1 (actual shipped Mk1 item)")

        log("DONE. Rebuild to cook. lr.RigEnable should stay 0/irrelevant (this is the STATIC path, "
            "no rig). Equip Mk1 in-game and check: correct SIZE (~110cm convention), barrel points "
            "FORWARD (not reversed), beam still exits near the muzzle end (LevelMuzzleOffsets NOT "
            "updated by this probe -- may be slightly off, that's expected/secondary for this test).")
