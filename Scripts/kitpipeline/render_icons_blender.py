r"""render_icons_blender.py -- render a 3/4 hero icon of EACH Mk's ACTUAL in-game body (Mk1-5 =
Tripo Rifle_Mk0N from Mk0N.fbx+Mk0N_tex; Mk6-10 = energy bodies from the picked glbs+_tex), on a
TRANSPARENT background at 512x512 RGBA, into Content/.../Icons/src/rifle_icon_MkN.png. mkicons.py
then imports them as T_LaserRifle_Icon_MkN (overwrite in place). Replaces the old "one Mk01 mesh
tinted 10 ways" icons so the MAM + inventory match the real per-Mk models.

Runs in the LIVE Blender session (GPU EEVEE, reliable) via mcp execute_blender_code that imports
this module and calls run(). Uses a TEMP scene + per-iteration orphans_purge and restores the user's
scene (memory hygiene). Game/build must be closed (GPU free)."""
import bpy, os, math, importlib.util
from mathutils import Vector

REPO = r"C:\Claude\Projects\SatisfactoryLaserRifleMod"
OUT = REPO + r"\Content\Equipment\LaserRifle\Icons\src"
TRIPO = REPO + r"\tripo_rifles"
ENERGY = TRIPO + r"\mk6to10_v31"
# Mk -> (mesh_file, tex_dir, body_id_for_flip_override)
SRC = {
    1:  (TRIPO + r"\Mk01.fbx", TRIPO + r"\Mk01_tex", "mk1"),
    2:  (TRIPO + r"\Mk02.fbx", TRIPO + r"\Mk02_tex", "mk2"),
    3:  (TRIPO + r"\Mk03.fbx", TRIPO + r"\Mk03_tex", "mk3"),
    4:  (TRIPO + r"\Mk04.fbx", TRIPO + r"\Mk04_tex", "mk4"),
    5:  (TRIPO + r"\Mk05.fbx", TRIPO + r"\Mk05_tex", "mk5"),
    6:  (ENERGY + r"\mk6_a.glb",  ENERGY + r"\_tex\mk6_a",  "mk6_a"),
    7:  (ENERGY + r"\mk7_c.glb",  ENERGY + r"\_tex\mk7_c",  "mk7_c"),
    8:  (ENERGY + r"\mk8_c.glb",  ENERGY + r"\_tex\mk8_c",  "mk8_c"),
    9:  (ENERGY + r"\mk9_c.glb",  ENERGY + r"\_tex\mk9_c",  "mk9_c"),
    10: (ENERGY + r"\mk10c2.glb", ENERGY + r"\_tex\mk10c2", "mk10c2"),   # new Mk10 (mk10_v2 pick, 2026-07-05)
}
# _align picks the forward end inconsistently on these abstract bodies: Mk1/Mk2 come out muzzle-LEFT
# while the other 8 are muzzle-RIGHT. Flip these so the whole icon set faces the SAME way (muzzle
# right), for a consistent inventory/MAM look. (Icon-only; does not touch the in-game body.)
ICON_FLIP = {1, 2}

def _frb():
    p = REPO + r"\Scripts\kitpipeline\finalize_raw_bodies_blender.py"
    s = importlib.util.spec_from_file_location("frb", p); m = importlib.util.module_from_spec(s); s.loader.exec_module(m); return m

def _wp(o): return [o.matrix_world @ v.co for v in o.data.vertices]

def _fit_camera(scene, cam, ob, fill=0.90, plo=3, phi=97):
    """Frame an ORTHO camera so the SUBJECT fills `fill` of the frame, based on the dense vertex core
    (plo..phi percentile in screen space) rather than the loose world bbox -- clips sparse outliers
    (thin wires, stray islands, hanging cables) that otherwise force a wide framing and leave the item
    tiny/unrecognizable at icon size (fixed 2026-07-12 after the Energy Cell canister rendered small).
    Keeps the camera's direction; recenters on the core + rescales ortho_scale. Requires the camera
    already positioned/rotated with a GENEROUS initial ortho_scale (object fully in view)."""
    import bpy_extras, numpy as np
    bpy.context.view_layer.update()
    mw = ob.matrix_world
    pr = [bpy_extras.object_utils.world_to_camera_view(scene, cam, mw @ v.co) for v in ob.data.vertices]
    xs = np.array([p.x for p in pr]); ys = np.array([p.y for p in pr])
    x0, x1 = np.percentile(xs, [plo, phi]); y0, y1 = np.percentile(ys, [plo, phi])
    cx, cy = (x0 + x1) / 2.0, (y0 + y1) / 2.0; half = max(x1 - x0, y1 - y0) / 2.0
    osc = cam.data.ortho_scale
    m3 = cam.matrix_world.to_3x3(); rt = m3.col[0].normalized(); up = m3.col[1].normalized()
    cam.location = cam.location + rt * ((cx - 0.5) * osc) + up * ((cy - 0.5) * osc)   # recenter core
    cam.data.ortho_scale = osc * (half * 2.0) / max(fill, 0.05)                        # fill core to `fill`
    bpy.context.view_layer.update()

def _import(sc, path):
    before = set(sc.objects.keys())
    if path.lower().endswith(".glb"): bpy.ops.import_scene.gltf(filepath=path)
    else: bpy.ops.import_scene.fbx(filepath=path)
    new = [bpy.data.objects[n] for n in sc.objects.keys() if n not in before]
    meshes = [o for o in new if o.type == 'MESH']
    for o in new:
        if o.type != 'MESH':
            try: bpy.data.objects.remove(o, do_unlink=True)
            except Exception: pass
    for o in sc.objects: o.select_set(False)
    for m in meshes: m.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1: bpy.ops.object.join()
    ob = bpy.context.view_layer.objects.active
    bpy.ops.object.parent_clear(type='CLEAR_KEEP_TRANSFORM')
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return ob

def _mat(name, texdir):
    m = bpy.data.materials.new("Icon_" + name); m.use_nodes = True
    nt = m.node_tree; nodes = nt.nodes; links = nt.links
    b = nodes.get("Principled BSDF")
    def T(fn, nc=False):
        p = os.path.join(texdir, fn)
        if not os.path.exists(p): return None
        img = bpy.data.images.load(p, check_existing=True)
        if nc: img.colorspace_settings.name = 'Non-Color'
        n = nodes.new("ShaderNodeTexImage"); n.image = img; return n
    base = T("Base Color.png")
    if base: links.new(base.outputs["Color"], b.inputs["Base Color"])
    r = T("Roughness.png", True)
    if r: links.new(r.outputs["Color"], b.inputs["Roughness"])
    mt = T("Metallic.png", True)
    if mt: links.new(mt.outputs["Color"], b.inputs["Metallic"])
    nr = T("Normal.png", True)
    if nr:
        nm = nodes.new("ShaderNodeNormalMap"); links.new(nr.outputs["Color"], nm.inputs["Color"])
        links.new(nm.outputs["Normal"], b.inputs["Normal"])
    # self-illuminate bright energy cores (pow(base,4)*strength): crushes dark metal, keeps cores.
    if base and "Emission Color" in b.inputs:
        g = nodes.new("ShaderNodeGamma"); g.inputs[1].default_value = 4.0
        links.new(base.outputs["Color"], g.inputs["Color"])
        links.new(g.outputs["Color"], b.inputs["Emission Color"])
        b.inputs["Emission Strength"].default_value = 3.0
    return m

def run(picks=None, log=print):
    frb = _frb(); os.makedirs(OUT, exist_ok=True)
    prev = bpy.context.window.scene
    tmp = bpy.data.scenes.new("LR_ICONS")
    bpy.context.window.scene = tmp
    w = bpy.data.worlds.new("IW"); tmp.world = w; w.use_nodes = True
    bg = w.node_tree.nodes.get("Background"); bg.inputs[0].default_value = (0.55, 0.57, 0.62, 1); bg.inputs[1].default_value = 1.1
    try: tmp.render.engine = 'BLENDER_EEVEE_NEXT'
    except Exception: tmp.render.engine = 'BLENDER_EEVEE'
    tmp.render.resolution_x = 512; tmp.render.resolution_y = 512
    tmp.render.film_transparent = True
    tmp.render.image_settings.file_format = 'PNG'; tmp.render.image_settings.color_mode = 'RGBA'
    def sun(rot, e):
        d = bpy.data.lights.new("S", 'SUN'); d.energy = e
        o = bpy.data.objects.new("S", d); tmp.collection.objects.link(o); o.rotation_euler = rot; return o
    lights = [sun((math.radians(55), math.radians(18), math.radians(40)), 4.5),
              sun((math.radians(65), 0, math.radians(-115)), 1.6)]
    done = []
    for mk in (picks if picks else sorted(SRC.keys())):
        path, texdir, bid = SRC[mk]
        ob = _import(tmp, path)
        frb._align(ob)
        if bid in frb.FLIP_OVERRIDES:
            p = _wp(ob); c = sum(p, Vector()) / len(p); frb._rot(ob, math.pi, 'Z', c)
        if mk in ICON_FLIP:
            p = _wp(ob); c = sum(p, Vector()) / len(p); frb._rot(ob, math.pi, 'Z', c)
        m = _mat("Mk%d" % mk, texdir); ob.data.materials.clear(); ob.data.materials.append(m)
        p = _wp(ob); cen = sum(p, Vector()) / len(p)
        size = max(max(q[i] for q in p) - min(q[i] for q in p) for i in range(3))
        cd = bpy.data.cameras.new("IC"); cd.type = 'ORTHO'; cd.ortho_scale = size * 2.6   # generous; _fit_camera tightens
        cd.clip_start = 0.001; cd.clip_end = size * 80
        cam = bpy.data.objects.new("IC", cd); tmp.collection.objects.link(cam)
        loc = cen + Vector((size * 1.3, -size * 1.7, size * 1.0))
        cam.location = loc; cam.rotation_euler = (cen - loc).to_track_quat('-Z', 'Y').to_euler()
        tmp.camera = cam
        _fit_camera(tmp, cam, ob)   # fill the frame on the dense subject core (no more small/loose icons)
        fp = os.path.join(OUT, "rifle_icon_Mk%d.png" % mk); tmp.render.filepath = fp
        bpy.ops.render.render(write_still=True); done.append(mk)
        log("icon Mk%d -> %s" % (mk, fp))
        bpy.data.objects.remove(cam, do_unlink=True)
        bpy.data.objects.remove(ob, do_unlink=True)
        bpy.data.orphans_purge(do_local_ids=True, do_linked_ids=True, do_recursive=True)
    for o in lights:
        try: bpy.data.objects.remove(o, do_unlink=True)
        except Exception: pass
    bpy.context.window.scene = prev
    try: bpy.data.scenes.remove(tmp)
    except Exception: pass
    bpy.data.orphans_purge(do_local_ids=True, do_linked_ids=True, do_recursive=True)
    return done
