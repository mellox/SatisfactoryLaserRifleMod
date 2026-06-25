# import_tripo.py -- import Tripo rifles (mesh + PBR material) as Mk1..Mk10 bodies.
# For each level it expects, under tripo_rifles/:
#     MkNN.fbx                       (the mesh)
#     MkNN_tex/Base Color.png        (albedo, sRGB)
#     MkNN_tex/Normal.png            (normal, linear)
#     MkNN_tex/Roughness.png         (linear)
#     MkNN_tex/Metallic.png          (linear)
# It imports the mesh (normalised to ~110cm), imports the 4 textures with correct
# colour-space/compression, builds M_Rifle_MkNN wiring them, assigns it to the mesh,
# then sets BP_Equip_LaserRifle.LevelBodyMeshes (carry-forward filled) + clears the
# flat BodyMaterial. Run (game+editor closed):
#   .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\ue\import_tripo.py
import unreal, os
ROOT="/LaserRifleMod"; EQ=ROOT+"/Equipment/LaserRifle"; MESH_DIR=EQ+"/Meshes_Tripo"
SRC=r"C:\Claude\Projects\SatisfactoryLaserRifleMod\tripo_rifles"
BP_PATH=EQ+"/BP_Equip_LaserRifle"
TARGET_CM=110.0
eal=unreal.EditorAssetLibrary; tools=unreal.AssetToolsHelpers.get_asset_tools()
MEL=unreal.MaterialEditingLibrary
def log(m): unreal.log_warning("LR_TRIPO | "+str(m))

if not eal.does_directory_exist(MESH_DIR): eal.make_directory(MESH_DIR)

# ---- mesh import (two-pass normalise) ----
def fbx_opts(scale):
    o=unreal.FbxImportUI()
    o.import_mesh=True; o.import_as_skeletal=False
    o.import_materials=False; o.import_textures=False     # we build the material ourselves
    o.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    sm=o.static_mesh_import_data
    sm.set_editor_property("combine_meshes", True)
    sm.set_editor_property("auto_generate_collision", True)
    sm.set_editor_property("import_uniform_scale", float(scale))
    return o

def import_mesh(fbx,name,scale):
    t=unreal.AssetImportTask()
    t.filename=fbx; t.destination_path=MESH_DIR; t.destination_name=name
    t.replace_existing=True; t.automated=True; t.save=True; t.options=fbx_opts(scale)
    tools.import_asset_tasks([t]); return MESH_DIR+"/"+name

def longest(path):
    m=eal.load_asset(path)
    if not m: return 0.0
    box=m.get_bounding_box(); mn=box.get_editor_property("min"); mx=box.get_editor_property("max")
    e=mx-mn; return max(e.x,e.y,e.z)

# ---- texture import ----
def import_tex(png, dest_dir, name, srgb, normal=False):
    t=unreal.AssetImportTask()
    t.filename=png; t.destination_path=dest_dir; t.destination_name=name
    t.replace_existing=True; t.automated=True; t.save=True
    tools.import_asset_tasks([t])
    tex=eal.load_asset(dest_dir+"/"+name)
    if tex:
        tex.set_editor_property("srgb", srgb)
        if normal:
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property("flip_green_channel", True)   # Blender/Tripo (OpenGL) -> UE
        eal.save_asset(dest_dir+"/"+name)
    return tex

# ---- material build ----
def build_material(level, base, normal, rough, metal):
    mpath=MESH_DIR+"/M_Rifle_Mk%02d"%level
    if eal.does_asset_exist(mpath): eal.delete_asset(mpath)
    mat=tools.create_asset("M_Rifle_Mk%02d"%level, MESH_DIR, unreal.Material, unreal.MaterialFactoryNew())
    def samp(tex, x, y, stype):
        n=MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSample, x, y)
        n.texture=tex; n.set_editor_property("sampler_type", stype); return n
    if base:
        b=samp(base,-450,-250, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        MEL.connect_material_property(b,"RGB", unreal.MaterialProperty.MP_BASE_COLOR)
        # --- Glow strips: emit where the texture is cyan (mask = B - R), in a
        #     GlowColor the weapon drives per level so it matches the beam. ---
        sub=MEL.create_material_expression(mat, unreal.MaterialExpressionSubtract, -300, 760)  # B - R (cyan-ness)
        MEL.connect_material_expressions(b, "B", sub, "A")
        MEL.connect_material_expressions(b, "R", sub, "B")
        gthr=MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -300, 900)
        gthr.set_editor_property("parameter_name", "GlowThreshold")
        gthr.set_editor_property("default_value", 0.35)
        cut=MEL.create_material_expression(mat, unreal.MaterialExpressionSubtract, -150, 760)   # (B-R) - threshold
        MEL.connect_material_expressions(sub, "", cut, "A")
        MEL.connect_material_expressions(gthr, "", cut, "B")
        mx=MEL.create_material_expression(mat, unreal.MaterialExpressionMax, -20, 760)          # max(.,0)
        mx.set_editor_property("const_b", 0.0)
        MEL.connect_material_expressions(cut, "", mx, "A")
        sharp=MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, 110, 760)  # *6 to sharpen
        sharp.set_editor_property("const_b", 6.0)
        MEL.connect_material_expressions(mx, "", sharp, "A")
        gint=MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, 110, 1000)
        gint.set_editor_property("parameter_name", "GlowIntensity")
        gint.set_editor_property("default_value", 4.0)
        gcol=MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, 110, 1140)
        gcol.set_editor_property("parameter_name", "GlowColor")
        gcol.set_editor_property("default_value", unreal.LinearColor(0.0, 1.0, 1.0, 1.0))
        m1=MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, 260, 820)     # mask*intensity
        MEL.connect_material_expressions(sharp, "", m1, "A")
        MEL.connect_material_expressions(gint, "", m1, "B")
        m2=MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, 420, 900)     # *color
        MEL.connect_material_expressions(m1, "", m2, "A")
        MEL.connect_material_expressions(gcol, "", m2, "B")
        # --- Racing-strip fire FX: a bright band travels along the glow strip per shot.
        #     Band = max(1 - |coord - PulsePos|*Sharpness, 0), confined to the existing cyan
        #     mask (`sharp`), tinted by GlowColor (`gcol`), ADDED on top of the steady glow
        #     (`m2`). PulseBand default 0 = perfect no-op until C++ raises it. PulseAxis flips
        #     U/V with no re-cook. Uses only Constant/Subtract/Abs/Multiply/Max/Add (all proven
        #     above) so the authoring run can't fail on an unavailable node class. ---
        # Strip coordinate: LOCAL position along the barrel (+X) normalized to 0..1 by the mesh's
        # X extent (PulseOriginX / PulseLength pushed from C++). UV-INDEPENDENT — the Tripo auto-
        # unwrap put the glow strip in a compact UV island with no along-strip axis, so a UV band
        # just pulsed the whole strip. Local X always runs muzzle-ward, so the band truly races.
        lpos=MEL.create_material_expression(mat, unreal.MaterialExpressionLocalPosition, -320, 1400)
        lx=MEL.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -160, 1400)
        lx.set_editor_property("r", True);  lx.set_editor_property("g", False)
        lx.set_editor_property("b", False); lx.set_editor_property("a", False)   # local X (barrel axis)
        MEL.connect_material_expressions(lpos, "", lx, "")
        porig=MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -160, 1520)
        porig.set_editor_property("parameter_name", "PulseOriginX"); porig.set_editor_property("default_value", 0.0)
        lsub=MEL.create_material_expression(mat, unreal.MaterialExpressionSubtract, 0, 1430)       # localX - OriginX
        MEL.connect_material_expressions(lx, "", lsub, "A")
        MEL.connect_material_expressions(porig, "", lsub, "B")
        plen=MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, 0, 1560)
        plen.set_editor_property("parameter_name", "PulseLength"); plen.set_editor_property("default_value", 100.0)
        coordn=MEL.create_material_expression(mat, unreal.MaterialExpressionDivide, 150, 1450)     # -> 0..1 along barrel
        MEL.connect_material_expressions(lsub, "", coordn, "A")
        MEL.connect_material_expressions(plen, "", coordn, "B")
        ppos=MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, 150, 1590)
        ppos.set_editor_property("parameter_name", "PulsePos"); ppos.set_editor_property("default_value", 0.0)
        dsub=MEL.create_material_expression(mat, unreal.MaterialExpressionSubtract, 320, 1470)     # coord - PulsePos
        MEL.connect_material_expressions(coordn, "", dsub, "A")
        MEL.connect_material_expressions(ppos, "", dsub, "B")
        dabs=MEL.create_material_expression(mat, unreal.MaterialExpressionAbs, 340, 1460)          # |.|
        MEL.connect_material_expressions(dsub, "", dabs, "")
        psharp=MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, 340, 1590)
        psharp.set_editor_property("parameter_name", "PulseSharpness"); psharp.set_editor_property("default_value", 16.0)
        dmul=MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, 480, 1480)     # |.|*Sharpness
        MEL.connect_material_expressions(dabs, "", dmul, "A")
        MEL.connect_material_expressions(psharp, "", dmul, "B")
        one=MEL.create_material_expression(mat, unreal.MaterialExpressionConstant, 480, 1600)
        one.set_editor_property("r", 1.0)
        inv=MEL.create_material_expression(mat, unreal.MaterialExpressionSubtract, 620, 1500)      # 1 - |.|*Sharpness
        MEL.connect_material_expressions(one, "", inv, "A")
        MEL.connect_material_expressions(dmul, "", inv, "B")
        band=MEL.create_material_expression(mat, unreal.MaterialExpressionMax, 760, 1500)          # slice 0..1 = max(.,0)
        band.set_editor_property("const_b", 0.0)
        MEL.connect_material_expressions(inv, "", band, "A")
        # Strip presence clamped to 0..1 (cyan mask `sharp` is 0..~3.9) so we only affect the strip.
        one2=MEL.create_material_expression(mat, unreal.MaterialExpressionConstant, 760, 1660)
        one2.set_editor_property("r", 1.0)
        strip01=MEL.create_material_expression(mat, unreal.MaterialExpressionMin, 900, 1600)       # min(sharp, 1)
        MEL.connect_material_expressions(sharp, "", strip01, "A")
        MEL.connect_material_expressions(one2, "", strip01, "B")
        a1=MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, 1040, 1500)      # slice confined to strip
        MEL.connect_material_expressions(band, "", a1, "A")
        MEL.connect_material_expressions(strip01, "", a1, "B")
        pband=MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, 1040, 1660)
        pband.set_editor_property("parameter_name", "PulseBand"); pband.set_editor_property("default_value", 0.0)
        ai=MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, 1200, 1540)      # slice * intensity
        MEL.connect_material_expressions(a1, "", ai, "A")
        MEL.connect_material_expressions(pband, "", ai, "B")
        # BRIGHT colour surge: a wide, intense, saturated band (PulseColor from C++) ADDED on top of
        # the glow. Brighter than the steady glow so the bloom can't wash it out — dark/dim bands kept
        # getting lost in the strip's bloom; a surge brighter than the strip is the reliable read.
        pcol=MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, 1200, 1680)
        pcol.set_editor_property("parameter_name", "PulseColor")
        pcol.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
        bcol=MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, 1360, 1560)    # * colour
        MEL.connect_material_expressions(ai, "", bcol, "A")
        MEL.connect_material_expressions(pcol, "", bcol, "B")
        emis_add=MEL.create_material_expression(mat, unreal.MaterialExpressionAdd, 600, 900)       # glow + surge
        MEL.connect_material_expressions(m2, "", emis_add, "A")
        MEL.connect_material_expressions(bcol, "", emis_add, "B")
        MEL.connect_material_property(emis_add, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if metal:
        mt=samp(metal,-450,40, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        MEL.connect_material_property(mt,"R", unreal.MaterialProperty.MP_METALLIC)
    if rough:
        r=samp(rough,-450,300, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        MEL.connect_material_property(r,"R", unreal.MaterialProperty.MP_ROUGHNESS)
    if normal:
        nm=samp(normal,-450,560, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        MEL.connect_material_property(nm,"RGB", unreal.MaterialProperty.MP_NORMAL)
    MEL.recompile_material(mat); eal.save_asset(mpath)
    return mat

def assign(mesh, mat, mesh_path):
    # Replace the slot array wholesale with a fresh StaticMaterial -- editing the
    # returned struct in place does NOT persist in UE Python, which left the mesh
    # still pointing at the FBX's (missing) tripo_mat -> default gray render.
    slots=mesh.get_editor_property("static_materials")
    slot_name = slots[0].get_editor_property("material_slot_name") if slots else unreal.Name("Mat0")
    new=unreal.StaticMaterial()
    new.set_editor_property("material_interface", mat)
    new.set_editor_property("material_slot_name", slot_name)
    mesh.set_editor_property("static_materials", [new])
    eal.save_asset(mesh_path)
    chk=eal.load_asset(mesh_path).get_editor_property("static_materials")
    cur=chk[0].get_editor_property("material_interface") if chk else None
    log("  slot[0] material now: " + (cur.get_name() if cur else "NONE"))

meshes=[None]*10
for i in range(1,11):
    fbx=os.path.join(SRC,"Mk%02d.fbx"%i)
    if not os.path.exists(fbx):
        log("Mk%02d MISSING %s -- skipped"%(i,fbx)); continue
    name="Rifle_Mk%02d"%i
    import_mesh(fbx,name,1.0)
    l1=longest(MESH_DIR+"/"+name)
    sc=round(TARGET_CM/l1,3) if l1>0 else 1.0
    import_mesh(fbx,name,sc)
    mesh=eal.load_asset(MESH_DIR+"/"+name)
    if not mesh: log("Mk%02d mesh FAIL"%i); continue

    texdir=os.path.join(SRC,"Mk%02d_tex"%i)
    tdest=MESH_DIR+"/Mk%02d_Tex"%i
    if not eal.does_directory_exist(tdest): eal.make_directory(tdest)
    def get(role, srgb, nrm=False):
        p=os.path.join(texdir, role+".png")
        return import_tex(p, tdest, "T_Rifle_Mk%02d_%s"%(i, role.replace(" ","")), srgb, nrm) if os.path.exists(p) else None
    base = get("Base Color", True)
    normal = get("Normal", False, True)
    rough = get("Roughness", False)
    metal = get("Metallic", False)
    mat = build_material(i, base, normal, rough, metal)
    assign(mesh, mat, MESH_DIR+"/"+name)
    meshes[i-1]=mesh
    log("Mk%02d %s scale=%s base=%s nrm=%s rough=%s metal=%s -> OK"%(
        i,name,sc,bool(base),bool(normal),bool(rough),bool(metal)))

valid=[m for m in meshes if m]
bp=eal.load_asset(BP_PATH)
if not bp:
    log("BLOCKER: BP not found "+BP_PATH)
elif len(valid)==0:
    log("No MkNN.fbx imported -- add at least Mk01.fbx + Mk01_tex/ and rerun")
else:
    filled=[None]*10; last=None
    for i in range(10):
        if meshes[i] is not None: last=meshes[i]
        filled[i]=last
    first=next(m for m in meshes if m)
    filled=[m if m is not None else first for m in filled]
    cdo=unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("LevelBodyMeshes", filled)
    cdo.set_editor_property("BodyMaterial", None)
    try: unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    except Exception as e: log("compile warn: "+str(e))
    unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
    log("DONE: %d textured rifles (filled to 10), BP saved. Rebuild to cook."%len(valid))
