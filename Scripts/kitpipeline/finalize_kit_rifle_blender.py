# finalize_kit_rifle_blender.py -- DETERMINISTIC, reusable Blender-side finishing pass for
# a composed kit rifle. No AI judgment calls -- same input always produces the same output.
# Replaces the ad-hoc import_kit_probe.py / import_kit_probe2.py / manual-rotation sequence
# from the Mk1 kitprobe iterations (2026-07-02) with one deterministic function.
#
# USAGE (run inside Blender via execute_blender_code, or as a Blender headless script):
#   import importlib.util
#   spec = importlib.util.spec_from_file_location("finalize_kit_rifle", <this path>)
#   mod = importlib.util.module_from_spec(spec); spec.loader.exec_module(mod)
#   mod.finalize("body_mk1_junk")   # reads kit_composition.json for this body automatically
#
# What this does, in order, ALL deterministic:
#   1. Load the body_id's assembly from its scratch scene (must already exist -- see note).
#   2. GEOMETRIC orientation fix (not label-based): find the long axis, measure cross-
#      sectional bbox area at both ends, the THINNER end is the muzzle, rotate around Z
#      until muzzle is at +X. No trust in body-prompt "forward zone" labels -- those are
#      for prompt-writing only, never for orientation ground truth.
#   3. Rigid-group rotate ALL related objects (body + parts + connectors + armature)
#      together around the world origin so relative placement is preserved.
#   4. Export one clean FBX (Stage-3 settings: no bake_space_transform, FBX_SCALE_NONE).
#   5. Return the per-slot material->source-part mapping (queried live from Blender data,
#      never hand-typed) for the UE-side script to consume.
#
# NOTE: this assumes the composed rifle's separated objects already exist in a scratch
# scene (as produced by the Phase-4 composition pass), named:
#   WORKBODY_<body_id>, WORKARM_<body_id>, PART_<part_id>_<mount>, CONN_<style>_<part_id>
# If starting fresh from kit_composed/fbx/Kit_<body_id>.fbx instead, import it first with
# merge_vertices=False so parts/connectors stay separable, using the same naming.

import bpy, mathutils, math, os

def _world_bbox(ob):
    mn = mathutils.Vector((1e9, 1e9, 1e9))
    mx = mathutils.Vector((-1e9, -1e9, -1e9))
    for corner in ob.bound_box:
        wc = ob.matrix_world @ mathutils.Vector(corner)
        mn.x = min(mn.x, wc.x); mn.y = min(mn.y, wc.y); mn.z = min(mn.z, wc.z)
        mx.x = max(mx.x, wc.x); mx.y = max(mx.y, wc.y); mx.z = max(mx.z, wc.z)
    return mn, mx

def _cross_section_area(mesh, mw, x_lo, x_hi):
    ys, zs = [], []
    for v in mesh.vertices:
        wp = mw @ v.co
        if x_lo <= wp.x <= x_hi:
            ys.append(wp.y); zs.append(wp.z)
    if not ys:
        return None
    return (max(ys) - min(ys)) * (max(zs) - min(zs))

def detect_and_fix_orientation(scene, body_ob, group_objs, log):
    """Deterministic: rotates group_objs (rigid, around world origin) so the mesh's
    long axis is world +X and the THINNER (muzzle) end faces +X. Returns total degrees
    rotated (for logging), does not guess from labels."""
    def longest_world_axis(ob):
        mn, mx = _world_bbox(ob)
        ext = mx - mn
        axis = max(range(3), key=lambda i: ext[i])
        return axis, ext

    axis, ext = longest_world_axis(body_ob)
    total_rot = 0.0

    # Step A: get the long axis onto X (0, 90, or -90 as needed; Z-axis rotations only --
    # these are held weapons, always roughly horizontal, never need X/Y axis correction).
    if axis == 1:  # long axis is Y -> rotate -90 around Z to bring it to X
        step = -90.0
    elif axis == 0:
        step = 0.0
    else:
        # long axis is Z (unusual for a held weapon) -- flag, do not guess further
        log("WARNING: long axis detected as Z (vertical) -- unusual for a held weapon, "
            "skipping automatic fix, human should inspect body_ob=%s" % body_ob.name)
        return 0.0

    if step != 0.0:
        _rigid_rotate_group(scene, group_objs, body_ob, step)
        total_rot += step

    # Step B: geometric muzzle check (thin end), regardless of what step A did
    mesh = body_ob.data
    mw = body_ob.matrix_world
    xs = [(mw @ v.co).x for v in mesh.vertices]
    xmin, xmax = min(xs), max(xs)
    span = xmax - xmin
    window = span * 0.15
    neg_area = _cross_section_area(mesh, mw, xmin, xmin + window)
    pos_area = _cross_section_area(mesh, mw, xmax - window, xmax)

    if neg_area is None or pos_area is None:
        log("WARNING: could not measure cross-section, skipping muzzle-direction check")
        return total_rot

    if neg_area < pos_area:
        # thin end (muzzle) is at -X -- flip 180
        _rigid_rotate_group(scene, group_objs, body_ob, 180.0)
        total_rot += 180.0
        log("Muzzle was at -X (area %.5f < %.5f) -- applied 180 flip" % (neg_area, pos_area))
    else:
        log("Muzzle confirmed at +X (area %.5f >= %.5f) -- no flip needed" % (pos_area, neg_area))

    return total_rot

def _rigid_rotate_group(scene, group_objs, active_ob, degrees, axis='Z'):
    bpy.context.window.scene = scene
    bpy.ops.object.select_all(action='DESELECT')
    for ob in group_objs:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = active_ob
    bpy.context.scene.cursor.location = (0, 0, 0)
    prev_pivot = bpy.context.scene.tool_settings.transform_pivot_point
    bpy.context.scene.tool_settings.transform_pivot_point = 'CURSOR'
    bpy.ops.transform.rotate(value=math.radians(degrees), orient_axis=axis)
    bpy.context.scene.tool_settings.transform_pivot_point = prev_pivot
    bpy.context.view_layer.update()

# The thin-end cross-section heuristic is right for 9/10 bodies but misreads mk8's slim
# adjustable stock as the muzzle (verified visually 2026-07-03: only flipped body in the
# orientation renders). Explicit per-body override of the heuristic's flip decision --
# a REAL data exception, recorded, not silently absorbed into a looser heuristic.
FLIP_OVERRIDES = {"body_mk8_high"}

def fine_align(scene, body_ob, group_objs, log):
    """The coarse pass only rotates in 90-degree steps, which left several barrels tilted
    10-25 degrees off +X (user-visible in-game: rifle doesn't point at the crosshair).
    Level THE BARREL LINE, not the silhouette: a first minimize-the-bbox attempt leveled
    the hull diagonal (stock-top to muzzle) and tilted every barrel DOWN instead (caught by
    the verify renders). The front ~40% of a rifle is barrel/handguard only -- no grip or
    stock -- so the line between two front vertex-slice centroids IS the barrel axis.
    Rotate (pitch about Y, yaw about Z) until that line is exactly along +X, with an
    apply-and-remeasure self-check so a sign-convention slip corrects itself instead of
    doubling the error."""
    def barrel_dir():
        mw = body_ob.matrix_world
        pts = [(mw @ v.co) for v in body_ob.data.vertices]
        xs = [p.x for p in pts]
        xmin, xmax = min(xs), max(xs)
        span = xmax - xmin
        a = [p for p in pts if p.x >= xmax - 0.08 * span]                       # muzzle tip slice
        b = [p for p in pts if xmax - 0.40 * span <= p.x <= xmax - 0.25 * span]  # mid-barrel slice
        if not a or not b:
            return None
        ca = mathutils.Vector((sum(p.x for p in a)/len(a), sum(p.y for p in a)/len(a), sum(p.z for p in a)/len(a)))
        cb = mathutils.Vector((sum(p.x for p in b)/len(b), sum(p.y for p in b)/len(b), sum(p.z for p in b)/len(b)))
        return ca - cb
    def level(axis, comp):
        d = barrel_dir()
        if d is None or abs(d.x) < 1e-6:
            return 0.0
        ang = math.degrees(math.atan2(comp(d), d.x))
        if abs(ang) < 0.2:
            return 0.0
        _rigid_rotate_group(scene, group_objs, body_ob, ang, axis=axis)
        d2 = barrel_dir()
        # self-check: if the residual grew, the sign convention was wrong -- rotate back double
        if d2 is not None and abs(comp(d2)) > abs(comp(d)):
            _rigid_rotate_group(scene, group_objs, body_ob, -2.0 * ang, axis=axis)
            ang = -ang
        return ang
    pitch = level('Y', lambda d: d.z)
    yaw = level('Z', lambda d: d.y)
    log("Fine-align (barrel-line): pitch %.1f deg, yaw %.1f deg" % (pitch, yaw))
    return yaw, pitch

def render_orientation_check(scene, body_ob, body_id, out_dir, log):
    """VERIFICATION artifact (part of the pipeline, not an afterthought): a flat-shaded side
    view of the finalized body. Muzzle must appear on the RIGHT. Written next to the review
    renders; look at it before trusting a batch."""
    png = os.path.join(out_dir, "%s_verify.png" % body_id)
    prev_nodes = scene.use_nodes
    scene.use_nodes = False
    sh = scene.display.shading
    prev_sh = (sh.light, sh.color_type, tuple(sh.single_color), sh.background_type, tuple(sh.background_color))
    sh.light = 'FLAT'; sh.color_type = 'SINGLE'; sh.single_color = (1.0, 0.45, 0.1)
    sh.background_type = 'VIEWPORT'; sh.background_color = (0.02, 0.02, 0.08)
    hidden = [(o, o.hide_render) for o in scene.objects if o is not body_ob]
    for o, _ in hidden: o.hide_render = True

    mw = body_ob.matrix_world
    pts = [mw @ v.co for v in body_ob.data.vertices]
    xs = [p.x for p in pts]; ys = [p.y for p in pts]; zs = [p.z for p in pts]
    cen = mathutils.Vector(((min(xs)+max(xs))/2, (min(ys)+max(ys))/2, (min(zs)+max(zs))/2))
    size = max(max(xs)-min(xs), max(ys)-min(ys), max(zs)-min(zs))
    cam_data = bpy.data.cameras.new("TMP_verify_cam"); cam_data.type = 'ORTHO'
    cam_data.ortho_scale = size * 1.25
    cam_data.clip_start = 0.01; cam_data.clip_end = size * 50
    cam = bpy.data.objects.new("TMP_verify_cam", cam_data)
    scene.collection.objects.link(cam)
    cam.location = cen + mathutils.Vector((0, -size * 2.0, 0))
    cam.rotation_euler = (math.pi / 2, 0, 0)
    prev_cam = scene.camera
    prev_r = (scene.render.resolution_x, scene.render.resolution_y, scene.render.filepath, scene.render.engine)
    scene.camera = cam
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.render.resolution_x = 640; scene.render.resolution_y = 360
    scene.render.filepath = png
    bpy.ops.render.render(write_still=True)
    scene.camera = prev_cam
    scene.render.resolution_x, scene.render.resolution_y, scene.render.filepath, scene.render.engine = prev_r
    for o, h in hidden:
        try: o.hide_render = h
        except Exception: pass
    bpy.data.objects.remove(cam, do_unlink=True)
    sh.light, sh.color_type, sh.single_color, sh.background_type, sh.background_color = prev_sh
    scene.use_nodes = prev_nodes
    log("Orientation verify render: %s" % png)
    return png

def query_slot_to_part_map(mesh_objs, body_ob, body_id):
    """NEVER hand-type this mapping -- always query live from the actual objects, so a
    typo or stale mapping can't silently ship wrong materials again. body_ob/body_id are
    passed in explicitly (already resolved by _find_body_object) rather than re-guessed
    from naming here, since the body's own name convention is inconsistent across bodies."""
    mapping = {}
    for ob in mesh_objs:
        if ob.type != 'MESH':
            continue
        part_id = None
        if ob is body_ob:
            part_id = body_id
        elif ob.name.startswith("PART_"):
            # PART_<part_id>_mount_N -> part_id (strip the trailing _mount_N)
            rest = ob.name[len("PART_"):]
            part_id = rest.rsplit("_mount_", 1)[0]
        elif ob.name.startswith("CONN_"):
            part_id = None  # connectors use the flat ConnectorMat, no source part
        for m in ob.data.materials:
            if m:
                mapping[m.name] = part_id  # None = flat/connector material
    return mapping

KIT_COMPOSED_DIR = r"C:\Claude\Projects\SatisfactoryLaserRifleMod\tripo_rifles\component_library\kit_composed\fbx"
SCRATCH_SCENE = "LR_KitFinalize"

def _find_by_prefix(scene, prefix):
    """Blender object names are globally unique across the whole .blend -- a re-import can
    land as 'WORKBODY_x.001' if stray data from an earlier run/scene is still alive. Match
    by prefix instead of exact name so this can't silently fail again."""
    for ob in scene.objects:
        if ob.name == prefix or ob.name.startswith(prefix + "."):
            return ob
    return None

def _find_body_object(scene, body_id):
    """The body mesh's naming convention is INCONSISTENT across the Phase-4 composition
    output (confirmed 2026-07-02): Mk1-3 used 'WORKBODY_<body_id>', Mk4-10 used a bare
    '<body_id>' with no prefix. Try both rather than assuming one -- this is real data
    variance, not something to paper over by renaming source files."""
    ob = _find_by_prefix(scene, "WORKBODY_" + body_id)
    if ob:
        return ob, "WORKBODY_" + body_id
    ob = _find_by_prefix(scene, body_id)
    if ob and ob.type == 'MESH':
        return ob, body_id
    return None, None

def _ensure_assembly_loaded(body_id, log):
    """Auto-import the Phase-4 composed FBX (Kit_<body_id>.fbx) fresh, with
    merge_vertices=False so PART_/CONN_/WORKBODY_ objects come back separated by name --
    no dependency on a scratch scene surviving between sessions."""
    scene = bpy.data.scenes.get(SCRATCH_SCENE)
    if scene and _find_body_object(scene, body_id)[0]:
        log("Reusing already-loaded assembly in scene '%s'." % SCRATCH_SCENE)
        return scene

    if scene:
        for ob in list(scene.objects):
            bpy.data.objects.remove(ob, do_unlink=True)
    else:
        scene = bpy.data.scenes.new(SCRATCH_SCENE)

    src = os.path.join(KIT_COMPOSED_DIR, "Kit_%s.fbx" % body_id)
    if not os.path.exists(src):
        log("BLOCKER: no composed FBX found at %s" % src)
        return None

    bpy.context.window.scene = scene
    bpy.ops.import_scene.fbx(filepath=src)
    body_ob, matched_name = _find_body_object(scene, body_id)
    if not body_ob:
        log("BLOCKER: imported %s but found neither 'WORKBODY_%s*' nor a bare '%s*' mesh "
            "object -- check naming convention matches Phase-4 composition output." % (
            src, body_id, body_id))
        return None
    log("Imported fresh assembly from %s (body object matched as '%s')" % (src, matched_name))
    return scene

def finalize(body_id, out_dir=None, log=print):
    scene = _ensure_assembly_loaded(body_id, log)
    if not scene:
        return None

    body_ob, matched_name = _find_body_object(scene, body_id)
    if not body_ob:
        log("BLOCKER: body object for %s not found in scene" % body_id)
        return None

    group_objs = [ob for ob in scene.objects if ob.type in ('MESH', 'ARMATURE')]

    total_rot = detect_and_fix_orientation(scene, body_ob, group_objs, log)
    log("Total rotation applied: %.0f degrees" % total_rot)

    # Known heuristic failure: invert the flip decision for bodies in FLIP_OVERRIDES.
    if body_id in FLIP_OVERRIDES:
        _rigid_rotate_group(scene, group_objs, body_ob, 180.0)
        log("FLIP OVERRIDE applied for %s (heuristic known-wrong, verified visually)" % body_id)

    # Fine alignment: barrel exactly along +X, not just within 90 degrees.
    fine_align(scene, body_ob, group_objs, log)

    mesh_objs = [ob for ob in group_objs if ob.type == 'MESH']
    bpy.context.window.scene = scene
    bpy.ops.object.select_all(action='DESELECT')
    for ob in mesh_objs:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = mesh_objs[0]
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)

    slot_map = query_slot_to_part_map(mesh_objs, body_ob, body_id)
    log("Slot->part map (queried live, not hand-typed): %s" % slot_map)

    if out_dir is None:
        out_dir = r"C:\Claude\Projects\SatisfactoryLaserRifleMod\tripo_rifles\component_library\kit_composed\fbx"

    # BODY-ONLY export (orientation already baked in via transform_apply above) -- this is what
    # RandomComponents needs as its base mesh: a bare, part-free body so dynamically-attached
    # loadout components are the ONLY parts visible, instead of stacking on top of parts already
    # baked into the composed assembly's geometry.
    bpy.ops.object.select_all(action='DESELECT')
    body_ob.select_set(True)
    bpy.context.view_layer.objects.active = body_ob
    body_only_out = os.path.join(out_dir, "Kit_%s_BODYONLY.fbx" % body_id)
    bpy.ops.export_scene.fbx(
        filepath=body_only_out, use_selection=True, object_types={'MESH'},
        apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE',
        bake_space_transform=False, mesh_smooth_type='FACE', add_leaf_bones=False,
        axis_forward='-Z', axis_up='Y', path_mode='COPY', embed_textures=True
    )
    log("Exported body-only: %s" % body_only_out)

    # Muzzle point as FRACTIONS of half-extent (fy, fz), NOT an absolute Blender-space value --
    # this sidesteps any Blender-meters vs UE-cm unit-conversion risk entirely (same proven
    # approach as the original Mk1-10 line's muzzle_fracs.json). FRONT-SLICE CENTROID, not
    # whole-bbox center: a boxy/asymmetric silhouette (bulk below, thin barrel on top) makes
    # the whole-mesh bbox center land well below the actual barrel tip ("beam under the
    # barrel" bug). Take only the front ~10% of the mesh's REAL vertices (not just the 8 AABB
    # corners) and average THEIR Y/Z -- dominated by the barrel's own thin cross-section, not
    # the bulkier body behind it. transform_apply above already baked the orientation fix into
    # the mesh data, so body_ob.data.vertices are already in the post-fix local space.
    verts = body_ob.data.vertices
    xs = [v.co.x for v in verts]
    max_x = max(xs)
    min_x = min(xs)
    slice_thresh = max_x - 0.10 * (max_x - min_x)   # front 10% of the mesh's length
    front_verts = [v.co for v in verts if v.co.x >= slice_thresh]
    ys = [v.co.y for v in verts]; zs = [v.co.z for v in verts]
    cen_y_whole = (min(ys) + max(ys)) / 2.0; half_y = (max(ys) - min(ys)) / 2.0
    cen_z_whole = (min(zs) + max(zs)) / 2.0; half_z = (max(zs) - min(zs)) / 2.0
    if front_verts:
        front_cen_y = sum(v.y for v in front_verts) / len(front_verts)
        front_cen_z = sum(v.z for v in front_verts) / len(front_verts)
    else:
        front_cen_y, front_cen_z = cen_y_whole, cen_z_whole   # degenerate fallback
    fy = (front_cen_y - cen_y_whole) / half_y if half_y > 1e-6 else 0.0
    fz = (front_cen_z - cen_z_whole) / half_z if half_z > 1e-6 else 0.0
    muzzle_frac = (fy, fz)   # apply to the UE-imported mesh's OWN bbox: cen + frac*half_extent
    log("Muzzle fraction (front-slice centroid, %d/%d verts in front 10%%, unitless fy/fz): %s" % (
        len(front_verts), len(verts), (muzzle_frac,)))

    # Full composed export (parts+connectors+body, existing behavior, unchanged).
    bpy.ops.object.select_all(action='DESELECT')
    for ob in mesh_objs:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = mesh_objs[0]
    out = os.path.join(out_dir, "Kit_%s_FINAL.fbx" % body_id)
    bpy.ops.export_scene.fbx(
        filepath=out, use_selection=True, object_types={'MESH'},
        apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE',
        bake_space_transform=False, mesh_smooth_type='FACE', add_leaf_bones=False,
        axis_forward='-Z', axis_up='Y', path_mode='COPY', embed_textures=True
    )
    log("Exported: %s" % out)

    # Verification render (muzzle must be on the RIGHT) -- pipeline artifact, review it.
    verify_dir = os.path.join(os.path.dirname(out_dir), "_review", "orient")
    os.makedirs(verify_dir, exist_ok=True)
    verify_png = render_orientation_check(scene, body_ob, body_id, verify_dir, log)

    return {"fbx": out, "body_only_fbx": body_only_out, "muzzle_frac": muzzle_frac,
            "slot_to_part": slot_map, "rotation_deg": total_rot, "verify_png": verify_png}
