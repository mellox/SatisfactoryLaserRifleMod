# mksmoke.py -- import the smoke sprite texture + build M_LaserRifle_Smoke.
# A translucent, unlit, two-sided sprite material whose shape comes from the
# wispy PNG (tripo_rifles/fx/smoke_puff.png). Params: SmokeColor (emissive tint =
# glow colour) and SmokeOpacity (overall thickness).
# Run (game+editor closed):
#   .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\ue\mksmoke.py
import unreal, os
EQ="/LaserRifleMod/Equipment/LaserRifle"; MAT=EQ+"/M_LaserRifle_Smoke"; TEX=EQ+"/T_LaserRifle_Smoke"
PNG=r"C:\Claude\Projects\SatisfactoryLaserRifleMod\tripo_rifles\fx\smoke_puff.png"
eal=unreal.EditorAssetLibrary; tools=unreal.AssetToolsHelpers.get_asset_tools(); MEL=unreal.MaterialEditingLibrary
def log(m): unreal.log_warning("LR_SMOKE | "+str(m))

# import the sprite texture (keep alpha)
t=unreal.AssetImportTask()
t.filename=PNG; t.destination_path=EQ; t.destination_name="T_LaserRifle_Smoke"
t.replace_existing=True; t.automated=True; t.save=True
tools.import_asset_tasks([t])
tex=eal.load_asset(TEX)
log("texture: "+("OK" if tex else "FAIL "+PNG))

if eal.does_asset_exist(MAT): eal.delete_asset(MAT)
mat=tools.create_asset("M_LaserRifle_Smoke", EQ, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("two_sided", True)

col=MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -520, -40)
col.set_editor_property("parameter_name","SmokeColor")
col.set_editor_property("default_value", unreal.LinearColor(1.0,1.0,1.0,1.0))
MEL.connect_material_property(col, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

smp=MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -560, 300)
if tex: smp.texture=tex
opac=MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -560, 520)
opac.set_editor_property("parameter_name","SmokeOpacity")
opac.set_editor_property("default_value", 0.5)
mul=MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -340, 360)
MEL.connect_material_expressions(smp, "A", mul, "A")     # sprite alpha = wispy shape
MEL.connect_material_expressions(opac, "", mul, "B")
MEL.connect_material_property(mul, "", unreal.MaterialProperty.MP_OPACITY)

MEL.recompile_material(mat); eal.save_asset(MAT)
log("M_LaserRifle_Smoke OK (sprite material; params SmokeColor, SmokeOpacity)")
