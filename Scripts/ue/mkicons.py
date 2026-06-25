# mkicons.py -- import the Blender-rendered rifle icons (Content/Equipment/LaserRifle/Icons/src/
# rifle_icon_MkN.png) as T_LaserRifle_Icon_MkN textures (overwrite in place so refs auto-update),
# and assign the inventory descriptor's small/big icon to the Mk1 hero. CDO-set + save, NO recompile.
# Run (game/editor/build closed): Scripts/ue/run_ue_python.ps1 -Script .\Scripts\ue\mkicons.py
import unreal, os
ROOT="/LaserRifleMod"; EQ=ROOT+"/Equipment/LaserRifle"; ICONS=EQ+"/Icons"
SRC=r"C:\Claude\Projects\SatisfactoryLaserRifleMod\Content\Equipment\LaserRifle\Icons\src"
eal=unreal.EditorAssetLibrary; tools=unreal.AssetToolsHelpers.get_asset_tools()
def log(m): unreal.log_warning("LR_ICONS | "+str(m))

def import_tex(png, name, srgb=True):
    t=unreal.AssetImportTask()
    t.filename=png; t.destination_path=ICONS; t.destination_name=name
    t.replace_existing=True; t.automated=True; t.save=True
    tools.import_asset_tasks([t])
    tex=eal.load_asset(ICONS+"/"+name)
    if tex:
        tex.set_editor_property("srgb", srgb)
        eal.save_asset(ICONS+"/"+name)
    return tex

count=0
for mk in range(1,11):
    png=os.path.join(SRC, "rifle_icon_Mk%d.png"%mk)
    if os.path.exists(png):
        if import_tex(png, "T_LaserRifle_Icon_Mk%d"%mk):
            count+=1; log("imported T_LaserRifle_Icon_Mk%d"%mk)
    else:
        log("MISSING %s"%png)

# inventory descriptor icon -> the Mk1 (emerald) hero render
hero=eal.load_asset(ICONS+"/T_LaserRifle_Icon_Mk1")
dbp=eal.load_asset("/LaserRifleMod/Equipment/LaserRifle/Desc_LaserRifle")
if dbp and hero:
    dc=unreal.get_default_object(dbp.generated_class())
    dc.set_editor_property("mSmallIcon", hero)
    dc.set_editor_property("mPersistentBigIcon", hero)
    eal.save_loaded_asset(dbp)   # CDO-set + save, NO compile_blueprint (would revert the CDO)
    log("descriptor icons assigned -> T_LaserRifle_Icon_Mk1")
else:
    log("WARN: descriptor or hero missing (dbp=%s hero=%s)"%(bool(dbp),bool(hero)))
log("DONE imported=%d"%count)
