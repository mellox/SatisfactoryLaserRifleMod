r"""
finalize_raw_bodies_blender.py -- Blender-side finalize for RAW generated rifle bodies
(single-mesh Tripo candidates, NOT the composed kit assemblies finalize_kit_rifle_blender.py
handles). Reuses the proven barrel-line align + front-slice muzzle-frac + verify render, but
skips all part composition (these bodies have no separate kit parts).

Per body: import glb -> join -> barrel-line align (long axis +X, muzzle +X, level pitch/yaw
via front vertex-slice centroids, with apply-and-remeasure self-check) -> verify render
(muzzle must be RIGHT) -> body-only FBX export -> front-slice-centroid muzzle_frac. Writes
one handoff JSON per body. Purges orphan datablocks each iteration (memory hygiene).

Call inside Blender:
  import importlib.util
  spec = importlib.util.spec_from_file_location("frb", <this path>)
  m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
  m.run([("mk6_a",6),("mk7_c",7),("mk8_c",8),("mk9_c",9),("mk10_a",10)])
"""
import bpy, os, math
from mathutils import Vector, Matrix

SRC = r"C:\Claude\Projects\SatisfactoryLaserRifleMod\tripo_rifles\mk6to10_v31"
OUT_FBX = r"C:\Claude\Projects\SatisfactoryLaserRifleMod\tripo_rifles\mk6to10_v31\final"
VERIFY = r"C:\Claude\Projects\SatisfactoryLaserRifleMod\tripo_rifles\mk6to10_v31\_review\orient"
HANDOFF = r"C:\Claude\Projects\SatisfactoryLaserRifleMod\Scripts\kitpipeline"
os.makedirs(OUT_FBX, exist_ok=True); os.makedirs(VERIFY, exist_ok=True)

def _wpts(ob):
    M = ob.matrix_world
    return [M @ v.co for v in ob.data.vertices]

def _rot(ob, ang, axis, pivot):
    ob.matrix_world = (Matrix.Translation(pivot) @ Matrix.Rotation(ang, 4, axis)
                       @ Matrix.Translation(-pivot)) @ ob.matrix_world

def _align(ob):
    p = _wpts(ob); cen = sum(p, Vector())/len(p)
    sx=max(q.x for q in p)-min(q.x for q in p); sy=max(q.y for q in p)-min(q.y for q in p); sz=max(q.z for q in p)-min(q.z for q in p)
    if sy>=sx and sy>=sz: _rot(ob, math.radians(-90),'Z',cen)
    elif sz>=sx and sz>=sy: _rot(ob, math.radians(90),'Y',cen)
    p=_wpts(ob); cen=sum(p,Vector())/len(p)
    xs=[q.x for q in p]; xmn,xmx=min(xs),max(xs); sp=xmx-xmn
    def area(lo,hi):
        ys=[q.y for q in p if lo<=q.x<=hi]; zs=[q.z for q in p if lo<=q.x<=hi]
        return (max(ys)-min(ys))*(max(zs)-min(zs)) if ys else 1e9
    if area(xmn,xmn+sp*0.15) < area(xmx-sp*0.15,xmx):
        _rot(ob, math.pi,'Z',cen)
    def bdir():
        pp=_wpts(ob); xs=[q.x for q in pp]; a,b=min(xs),max(xs); s=b-a
        A=[q for q in pp if q.x>=b-0.08*s]; B=[q for q in pp if b-0.40*s<=q.x<=b-0.25*s]
        if not A or not B: return None
        return (sum(A,Vector())/len(A))-(sum(B,Vector())/len(B))
    for axis,comp in (('Y',lambda d:d.z),('Z',lambda d:d.y)):
        d=bdir()
        if d and abs(d.x)>1e-6:
            ang=math.atan2(comp(d),d.x)
            if math.radians(0.3)<abs(ang)<math.radians(60):
                p=_wpts(ob); cen=sum(p,Vector())/len(p)
                _rot(ob,ang,axis,cen)
                d2=bdir()
                if d2 and abs(comp(d2))>abs(comp(d)):
                    p=_wpts(ob); cen=sum(p,Vector())/len(p); _rot(ob,-2*ang,axis,cen)
    # ROLL leveling (the previously-missing 3rd axis -- pitch/yaw above left the rifle able to
    # sit rotated about its own barrel). Roll about X to MAXIMIZE the vertical (Z) extent so the
    # rifle sits FLAT/upright in side view (a rifle's tall silhouette is its side profile).
    # This settles roll to within 180 deg; the up/down (grip-down) ambiguity is handled per-body
    # by ROLL_FLIP below, confirmed from the textured verify renders.
    p=_wpts(ob); cen=sum(p,Vector())/len(p)
    best_ang, best_ext = 0.0, -1.0
    for deg in range(-90, 90, 2):
        a=math.radians(deg); ca,sa=math.cos(a),math.sin(a)
        zs=[(q.z-cen.z)*ca-(q.y-cen.y)*sa for q in p]
        ext=max(zs)-min(zs)
        if ext>best_ext: best_ext, best_ang = ext, deg
    if abs(best_ang)>0.5:
        _rot(ob, math.radians(best_ang), 'X', cen)

# The thin-end cross-section heuristic misreads mk8_c's twin-tube emitter as the stock and
# points it backwards in-game (verified textured render + [LR] muzzleX log 2026-07-05). Same
# class of failure as the composed-pipeline's FLIP_OVERRIDES={"body_mk8_high"}; recorded as a
# per-body exception, not smoothed into a looser heuristic. 180 deg about Z (yaw) swaps the
# emitter to +X.
FLIP_OVERRIDES = {"mk8_c"}
# Per-body 180-deg ROLL flip (grip must end DOWN). The roll-maximize pass settles roll to
# within 180; these bodies come out upside-down and need the flip. Confirmed from the textured
# verify renders (populated after the first roll-enabled finalize + review).
ROLL_FLIP = set()
# Per-body muzzle-frac (fy, fz) HAND OVERRIDE. mk8_c (user 2026-07-05, textured EEVEE renders +
# vertex measurement): the emitter is a THREE-TUBE cluster with the MIDDLE tube protruding
# furthest forward as the barrel. The auto front-8% slice lands high AND laterally off (fy=-0.11)
# because it catches asymmetric tube geometry; the earlier (0,0) override put the muzzle at the
# WHOLE-BODY center, which the downward grip drags BELOW the tubes. Measured frontmost-tip center
# = laterally CENTERED, fz ~ +0.20 (the protruding middle-tube bore). Beam now leaves that bore.
MUZZLE_FRAC_OVERRIDE = {"mk8_c": (0.0, 0.20)}

# Per-body ENDPOINT LEVELING -- DISABLED (regressed mk6_a, in-game 2026-07-05). The idea: level a
# full-width body using the TAIL(15%)->NOSE(15%) vector when _align's front-vs-mid barrel heuristic
# gets no signal. FATAL FLAW proven by textured before/after renders: the TAIL 15% includes the
# GRIP/STOCK (which hangs low and back on any rifle), so the tail centroid is dragged DOWN -> the
# tail->nose vector reads a false "+10.75 nose high" -> the "correction" rotated an ALREADY-LEVEL
# mk6 11 deg nose-DOWN. Endpoint leveling is unusable for anything with a grip in the tail (i.e.
# every rifle). Left the function for archaeology; the front-slice _align alone is correct for these.
ENDPOINT_LEVEL = set()   # was {"mk6_a","mk7_c"} -- reverted, see above

def _level_endpoints(ob, log=print):
    def _resid():
        p = _wpts(ob); xs = [q.x for q in p]; a, b = min(xs), max(xs); s = max(b - a, 1e-9)
        fr = [q for q in p if q.x >= b - 0.15 * s]; bk = [q for q in p if q.x <= a + 0.15 * s]
        return (sum(fr, Vector()) / len(fr)) - (sum(bk, Vector()) / len(bk)), p
    # pitch about Y: +atan2(dz,dx) drives the tail->nose dz to 0 (nose to level)
    d, p = _resid(); ap = math.atan2(d.z, d.x)
    if abs(ap) > math.radians(0.4):
        _rot(ob, ap, 'Y', sum(p, Vector()) / len(p)); log("  endpoint pitch %+.2f" % math.degrees(ap))
    # yaw about Z: -atan2(dy,dx) drives dy to 0 (nose straight ahead)
    d, p = _resid(); ay = -math.atan2(d.y, d.x)
    if abs(ay) > math.radians(0.4):
        _rot(ob, ay, 'Z', sum(p, Vector()) / len(p)); log("  endpoint yaw %+.2f" % math.degrees(ay))

def _muzzle_frac(ob):
    # apply transforms into mesh data first, then front-slice centroid on local verts
    for o in bpy.context.scene.objects: o.select_set(False)
    ob.select_set(True); bpy.context.view_layer.objects.active = ob
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    vs = ob.data.vertices
    xs=[v.co.x for v in vs]; ys=[v.co.y for v in vs]; zs=[v.co.z for v in vs]
    # muzzle = front-slice geometric CENTER (min+max)/2, NOT the mass-centroid. User insight
    # 2026-07-05: on bodies whose shell runs full-width to the front (Mk8 twin-tubes, Mk6/Mk7 --
    # no thin protruding barrel), the CENTROID is pulled UP by the top bulk, so the muzzle sat
    # above the real bore and the beam raked down. The geometric CENTER lands on the bore whether
    # the front is a thin barrel (slice = just the barrel, center = its middle) or a full face
    # (center = the face middle ~ the bore). Front 8% slice for a stable sample.
    mx=max(xs); mn=min(xs); thr=mx-0.08*(mx-mn)
    front=[v.co for v in vs if v.co.x>=thr]
    cy=(min(ys)+max(ys))/2; hy=(max(ys)-min(ys))/2
    cz=(min(zs)+max(zs))/2; hz=(max(zs)-min(zs))/2
    if front:
        fy=(min(v.y for v in front)+max(v.y for v in front))/2
        fz=(min(v.z for v in front)+max(v.z for v in front))/2
    else:
        fy,fz=cy,cz
    return (round((fy-cy)/hy,4) if hy>1e-6 else 0.0,
            round((fz-cz)/hz,4) if hz>1e-6 else 0.0)

def _verify_render(ob, body_id):
    scene=bpy.context.scene
    prev_nodes=scene.use_nodes; scene.use_nodes=False
    sh=scene.display.shading
    ps=(sh.light,sh.color_type,tuple(sh.single_color),sh.background_type,tuple(sh.background_color))
    sh.light='FLAT'; sh.color_type='SINGLE'; sh.single_color=(0.8,0.65,0.2)
    sh.background_type='VIEWPORT'; sh.background_color=(0.03,0.03,0.05)
    hid=[(o,o.hide_render) for o in scene.objects if o is not ob]
    for o,_ in hid: o.hide_render=True
    p=_wpts(ob); cen=sum(p,Vector())/len(p)
    size=max(max(q.x for q in p)-min(q.x for q in p),max(q.y for q in p)-min(q.y for q in p),max(q.z for q in p)-min(q.z for q in p))
    cd=bpy.data.cameras.new("VC"); cd.type='ORTHO'; cd.ortho_scale=size*1.25; cd.clip_start=0.01; cd.clip_end=size*50
    cam=bpy.data.objects.new("VC",cd); scene.collection.objects.link(cam)
    cam.location=cen+Vector((0,-size*2,0)); cam.rotation_euler=(math.pi/2,0,0)
    pc=scene.camera; pr=(scene.render.resolution_x,scene.render.resolution_y,scene.render.filepath,scene.render.engine)
    scene.camera=cam; scene.render.engine='BLENDER_WORKBENCH'
    scene.render.resolution_x=640; scene.render.resolution_y=360
    png=os.path.join(VERIFY,"%s_verify.png"%body_id); scene.render.filepath=png
    bpy.ops.render.render(write_still=True)
    scene.camera=pc; scene.render.resolution_x,scene.render.resolution_y,scene.render.filepath,scene.render.engine=pr
    for o,h in hid:
        try: o.hide_render=h
        except Exception: pass
    bpy.data.objects.remove(cam,do_unlink=True)
    sh.light,sh.color_type,sh.single_color,sh.background_type,sh.background_color=ps
    scene.use_nodes=prev_nodes
    return png

def run(picks, log=print):
    import json
    scene=bpy.context.window.scene
    results=[]
    for name, mk in picks:
        glb=os.path.join(SRC,name+".glb")
        for o in scene.objects: o.select_set(False)
        before=set(scene.objects.keys())
        bpy.ops.import_scene.gltf(filepath=glb)
        new=[bpy.data.objects[n] for n in scene.objects.keys() if n not in before]
        meshes=[o for o in new if o.type=='MESH']
        for o in new:
            if o.type!='MESH':
                try: bpy.data.objects.remove(o,do_unlink=True)
                except Exception: pass
        for o in scene.objects: o.select_set(False)
        for m in meshes: m.select_set(True)
        bpy.context.view_layer.objects.active=meshes[0]
        if len(meshes)>1: bpy.ops.object.join()
        ob=bpy.context.view_layer.objects.active
        ob.name="RAWBODY_%s"%name
        bpy.ops.object.parent_clear(type='CLEAR_KEEP_TRANSFORM')
        bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
        _align(ob)
        if name in FLIP_OVERRIDES:
            p=_wpts(ob); cen=sum(p,Vector())/len(p)
            _rot(ob, math.pi, 'Z', cen)
            log("FLIP OVERRIDE applied for %s (heuristic points it backwards)" % name)
        if name in ROLL_FLIP:
            p=_wpts(ob); cen=sum(p,Vector())/len(p)
            _rot(ob, math.pi, 'X', cen)
            log("ROLL FLIP applied for %s (was upside-down)" % name)
        if name in ENDPOINT_LEVEL:
            _level_endpoints(ob, log)
            log("ENDPOINT LEVEL applied for %s (full-width body; barrel heuristic had no signal)" % name)
        mfrac=_muzzle_frac(ob)   # applies transforms
        if name in MUZZLE_FRAC_OVERRIDE:
            mfrac = MUZZLE_FRAC_OVERRIDE[name]
            log("MUZZLE OVERRIDE %s -> %s (auto front-slice couldn't find the bore)" % (name, mfrac))
        vpng=_verify_render(ob, name)
        # body-only FBX (single mesh, embedded textures)
        for o in scene.objects: o.select_set(False)
        ob.select_set(True); bpy.context.view_layer.objects.active=ob
        fbx=os.path.join(OUT_FBX,"Body_%s.fbx"%name)
        bpy.ops.export_scene.fbx(filepath=fbx, use_selection=True, object_types={'MESH'},
            apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE',
            bake_space_transform=False, mesh_smooth_type='FACE', add_leaf_bones=False,
            path_mode='COPY', embed_textures=True)
        ho={"body_id":name,"mk_index":mk,"body_only_fbx":fbx,"muzzle_frac":list(mfrac),
            "src_glb":glb,"verify_png":vpng}
        with open(os.path.join(HANDOFF,"_rawhandoff_%s.json"%name),"w") as f:
            json.dump(ho,f,indent=1)
        results.append({"name":name,"mk":mk,"muzzle_frac":mfrac,"fbx_ok":os.path.exists(fbx)})
        log("%s mk%d muzzle_frac=%s fbx=%s" % (name,mk,mfrac,os.path.exists(fbx)))
        bpy.data.objects.remove(ob,do_unlink=True)
        bpy.data.orphans_purge(do_local_ids=True,do_linked_ids=True,do_recursive=True)
    return results
