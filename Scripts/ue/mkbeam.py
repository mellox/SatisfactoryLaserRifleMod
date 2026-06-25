# mkbeam.py -- creates /LaserRifleMod/Equipment/LaserRifle/M_LaserRifle_Beam
# An UNLIT, two-sided emissive material so the laser beam shows its color in any
# lighting (the lit body material rendered black as a beam). Params:
#   BeamColor (vector)  Intensity (scalar)   emissive = BeamColor * Intensity
# Headless: .\Scripts\ue\run_ue_python.ps1 -Script .\Scripts\ue\mkbeam.py
import unreal
ROOT = "/LaserRifleMod"; EQ = ROOT + "/Equipment/LaserRifle"
MAT  = EQ + "/M_LaserRifle_Beam"
eal  = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MEL  = unreal.MaterialEditingLibrary
def log(m): unreal.log_warning("LR_BEAM | " + str(m))

if not eal.does_directory_exist(EQ): eal.make_directory(EQ)
if eal.does_asset_exist(MAT): eal.delete_asset(MAT)
mat = tools.create_asset("M_LaserRifle_Beam", EQ, unreal.Material, unreal.MaterialFactoryNew())

mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("two_sided", True)

col = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -800, 0)
col.set_editor_property("parameter_name", "BeamColor")
col.set_editor_property("default_value", unreal.LinearColor(0.18, 0.80, 0.44, 1.0))
inten = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -800, 200)
inten.set_editor_property("parameter_name", "Intensity")
inten.set_editor_property("default_value", 50.0)
mul = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -450, 50)
MEL.connect_material_expressions(col, "", mul, "A")
MEL.connect_material_expressions(inten, "", mul, "B")
MEL.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

MEL.recompile_material(mat)
eal.save_asset(MAT)
disk = r"C:\Claude\Projects\SatisfactoryModLoader\Mods\LaserRifleMod\Content\Equipment\LaserRifle\M_LaserRifle_Beam.uasset"
import os
log("M_LaserRifle_Beam OK (unlit; params BeamColor, Intensity). on disk: " +
    (("YES %d bytes" % os.path.getsize(disk)) if os.path.exists(disk) else "NOT FOUND (will cook from /Game mount)"))
