r"""arc_preview_blender.py -- design the electrical-arc LOOK in Blender (EEVEE), zero UE builds.
Generates a BRANCHING lightning bolt via recursive midpoint-displacement + forks (the same algorithm
we'll bake into C++), builds emissive tube geometry, and renders it on a dark background so we can
iterate branch density / chaos / thickness / glow before committing to a build. Call run() via the
live Blender session. Tune the PRESETS, re-render, look; then I port the winning numbers to
LaserRifleWeapon UpdateArcs. (Segment COUNT here maps to SEGS_PER_ARC/branch budget in C++.)"""
import bpy, math, os
from mathutils import Vector, Matrix

OUT = r"C:\Users\mello\AppData\Local\Temp\claude\C--Claude-Projects\d5a7ef55-e564-4109-aa3c-33547ce61346\scratchpad\arcprev"

def _rand(scene_seed):
    # deterministic-ish LCG so renders are reproducible per seed (Date/random unavailable rules don't
    # apply in Blender, but a seeded generator keeps previews stable while tuning)
    state = [scene_seed & 0x7fffffff]
    def r():
        state[0] = (1103515245 * state[0] + 12345) & 0x7fffffff
        return state[0] / 0x7fffffff
    return r

def gen_bolt(a, b, chaos, gens, allow_branch, weight, rnd, out):
    if gens <= 0 or (b - a).length < 0.02:
        out.append((a.copy(), b.copy(), weight)); return
    d = b - a; L = d.length; u = d.normalized()
    perp = u.cross(Vector((0, 0, 1)))
    if perp.length < 1e-6: perp = u.cross(Vector((1, 0, 0)))
    perp.normalize(); perp2 = u.cross(perp).normalized()
    amp = chaos * L * 0.5
    mid = (a + b) * 0.5 + perp * (rnd() * 2 - 1) * amp + perp2 * (rnd() * 2 - 1) * amp
    gen_bolt(a, mid, chaos, gens - 1, allow_branch, weight, rnd, out)
    gen_bolt(mid, b, chaos, gens - 1, allow_branch, weight, rnd, out)
    if allow_branch and rnd() < 0.6:
        ang = (0.35 + rnd() * 0.55) * (1 if rnd() < 0.5 else -1)   # ~20-52 deg off
        axis = perp if rnd() < 0.5 else perp2
        bdir = (Matrix.Rotation(ang, 4, axis) @ u)
        blen = L * (0.5 + rnd() * 0.7)
        gen_bolt(mid, mid + bdir * blen, chaos * 1.1, gens - 1, rnd() < 0.35, weight * 0.6, rnd, out)

def _emat(name, color, strength):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes.get("Principled BSDF")
    if "Emission Color" in b.inputs:
        b.inputs["Emission Color"].default_value = (*color, 1); b.inputs["Emission Strength"].default_value = strength
    b.inputs["Base Color"].default_value = (*color, 1)
    return m

def run(presets=None, log=print):
    os.makedirs(OUT, exist_ok=True)
    presets = presets or [
        # name        chaos  gens  thick  core_str  branchcol
        ("A_tight",   0.16,  6,    0.9,   40.0),
        ("B_wild",    0.28,  6,    1.3,   40.0),
        ("C_dense",   0.22,  7,    1.1,   55.0),
    ]
    prev = bpy.context.window.scene
    tmp = bpy.data.scenes.new("ARC_PREV"); bpy.context.window.scene = tmp
    w = bpy.data.worlds.new("W"); tmp.world = w; w.use_nodes = True
    bg = w.node_tree.nodes.get("Background"); bg.inputs[0].default_value = (0.01, 0.01, 0.02, 1); bg.inputs[1].default_value = 0.0
    try: tmp.render.engine = 'BLENDER_EEVEE_NEXT'
    except Exception: tmp.render.engine = 'BLENDER_EEVEE'
    tmp.render.resolution_x = 700; tmp.render.resolution_y = 500
    # (Blender 5.1 moved the scene compositor API; skip the glare node -- bright emission on a dark bg
    #  reads as lightning on its own. Bloom is a post effect in-game anyway.)
    core_mat = _emat("ArcCore", (0.75, 0.85, 1.0), 40.0)
    branch_mat = _emat("ArcBranch", (0.55, 0.65, 1.0), 22.0)
    imgs = []
    for name, chaos, gens, thick, cstr in presets:
        core_mat.node_tree.nodes.get("Principled BSDF").inputs["Emission Strength"].default_value = cstr
        segs = []
        gen_bolt(Vector((-1.6, 0, 0)), Vector((1.6, 0, 0.15)), chaos, gens, True, 1.0, _rand(hash(name) & 0xffff), segs)
        made = []
        for (p0, p1, wt) in segs:
            d = p1 - p0; L = d.length
            if L < 1e-4: continue
            bpy.ops.mesh.primitive_cylinder_add(vertices=6, radius=thick * 0.01 * (1.0 if wt > 0.8 else 0.6),
                depth=L, location=(p0 + p1) * 0.5)
            cyl = bpy.context.active_object
            cyl.rotation_euler = d.to_track_quat('Z', 'Y').to_euler()
            cyl.data.materials.append(core_mat if wt > 0.8 else branch_mat); made.append(cyl)
        # camera
        cd = bpy.data.cameras.new("C"); cd.type = 'ORTHO'; cd.ortho_scale = 4.0
        cam = bpy.data.objects.new("C", cd); tmp.collection.objects.link(cam)
        cam.location = (0, -6, 0.1); cam.rotation_euler = (math.pi / 2, 0, 0); tmp.camera = cam
        fp = os.path.join(OUT, "arc_%s.png" % name); tmp.render.filepath = fp
        bpy.ops.render.render(write_still=True); imgs.append(fp); log("rendered " + name + " segs=%d" % len(segs))
        for o in made + [cam]:
            try: bpy.data.objects.remove(o, do_unlink=True)
            except Exception: pass
    bpy.context.window.scene = prev
    try: bpy.data.scenes.remove(tmp)
    except Exception: pass
    bpy.data.orphans_purge(do_local_ids=True, do_linked_ids=True, do_recursive=True)
    return imgs


def gen_burst(center, k, length, chaos, gens, rnd, out):
    """RADIAL BURST: k jagged tendrils from `center` in random 3D directions -> a small electric
    spark-BALL (the reference "spark that floats away" look), not a directional bolt. This is what the
    Mk4-10 floating sparks bake into (LaserRifleWeapon UpdateArcs render loop: per-arc, re-diced every
    frame from the drifting origin). k*segs is bounded by the C++ MAX_SEGS_PER_ARC budget."""
    for _ in range(k):
        z = rnd() * 2 - 1; t = rnd() * 2 * math.pi; r = math.sqrt(max(0.0, 1 - z * z))
        d = Vector((r * math.cos(t), r * math.sin(t), z))
        gen_bolt(center.copy(), center + d * length * (0.55 + rnd() * 0.6), chaos, gens, True, 1.0, rnd, out)


def run_burst(presets=None, log=print):
    """Preview the BURST spark (blue-purple tendrils + white-hot core on dark bg). Winner ported to C++
    was ~K=16 gens=2 (burst_E_K28). In-game bloom whitens the dense core; here we lower emission so the
    blue-purple reads. Motion ("floats away") is C++/in-game only -- this validates SHAPE + COLOR."""
    os.makedirs(OUT, exist_ok=True)
    presets = presets or [
        # name          K   length chaos gens thick
        ("K16_med",     16, 1.05,  0.20, 2,   0.0075),
        ("K22_dense",   22, 1.00,  0.19, 3,   0.0065),
    ]
    prev = bpy.context.window.scene
    tmp = bpy.data.scenes.new("BURST_PREV"); bpy.context.window.scene = tmp
    w = bpy.data.worlds.new("BW"); tmp.world = w; w.use_nodes = True
    bg = w.node_tree.nodes.get("Background"); bg.inputs[0].default_value = (0.01, 0.01, 0.02, 1); bg.inputs[1].default_value = 0.0
    try: tmp.render.engine = 'BLENDER_EEVEE_NEXT'
    except Exception: tmp.render.engine = 'BLENDER_EEVEE'
    tmp.render.resolution_x = 600; tmp.render.resolution_y = 600
    core_mat = _emat("BurstCore", (0.80, 0.85, 1.0), 9.0)      # white-blue hot core
    branch_mat = _emat("BurstBranch", (0.42, 0.32, 1.0), 5.0)  # blue-purple tendrils
    imgs = []
    for name, k, length, chaos, gens, thick in presets:
        segs = []
        gen_burst(Vector((0, 0, 0)), k, length, chaos, gens, _rand(hash(name) & 0xffff), segs)
        made = []
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.07, location=(0, 0, 0))
        core = bpy.context.active_object; core.data.materials.append(core_mat); made.append(core)
        for (p0, p1, wt) in segs:
            d = p1 - p0; L = d.length
            if L < 1e-4: continue
            bpy.ops.mesh.primitive_cylinder_add(vertices=6, radius=thick * (1.0 if wt > 0.8 else 0.62),
                depth=L, location=(p0 + p1) * 0.5)
            cyl = bpy.context.active_object
            cyl.rotation_euler = d.to_track_quat('Z', 'Y').to_euler()
            cyl.data.materials.append(core_mat if wt > 0.8 else branch_mat); made.append(cyl)
        cd = bpy.data.cameras.new("BC"); cd.type = 'ORTHO'; cd.ortho_scale = 3.6
        cam = bpy.data.objects.new("BC", cd); tmp.collection.objects.link(cam)
        cam.location = (0, -6, 0.0); cam.rotation_euler = (math.pi / 2, 0, 0); tmp.camera = cam
        fp = os.path.join(OUT, "burst_%s.png" % name); tmp.render.filepath = fp
        bpy.ops.render.render(write_still=True); imgs.append(fp); log("rendered burst " + name + " segs=%d" % len(segs))
        for o in made + [cam]:
            try: bpy.data.objects.remove(o, do_unlink=True)
            except Exception: pass
    bpy.context.window.scene = prev
    try: bpy.data.scenes.remove(tmp)
    except Exception: pass
    bpy.data.orphans_purge(do_local_ids=True, do_linked_ids=True, do_recursive=True)
    return imgs
