import unreal

SRC = "/LaserRifleMod/Audio/Events/Default_Work_Unit/Play_LaserRifle_Fire"
DST_PKG = "/LaserRifleMod/Audio/Play_LaserRifle_Fire"
DST_NAME = "Play_LaserRifle_Fire"

ar = unreal.AssetRegistryHelpers.get_asset_registry()
tools = unreal.AssetToolsHelpers.get_asset_tools()

def log(m): unreal.log("LR_SND " + m)

if not unreal.EditorAssetLibrary.does_asset_exist(SRC):
    log("FATAL src missing: " + SRC)
else:
    log("src exists: " + SRC)
    if unreal.EditorAssetLibrary.does_asset_exist(DST_PKG):
        log("dst already exists, skipping rename: " + DST_PKG)
    else:
        ren = unreal.AssetRenameData(
            asset=unreal.EditorAssetLibrary.load_asset(SRC),
            new_package_path="/LaserRifleMod/Audio",
            new_name=DST_NAME,
        )
        tools.rename_assets([ren])
        log("rename invoked -> " + DST_PKG)

    # Verify + report the event's class and that it loads
    if unreal.EditorAssetLibrary.does_asset_exist(DST_PKG):
        obj = unreal.EditorAssetLibrary.load_asset(DST_PKG)
        log("dst exists; class=" + (obj.get_class().get_name() if obj else "<none>"))
    else:
        log("WARN dst not found after rename: " + DST_PKG)

    # Save everything (and clean up redirector by saving the source dir too)
    unreal.EditorAssetLibrary.save_directory("/LaserRifleMod/Audio", only_if_is_dirty=False, recursive=True)
    log("saved /LaserRifleMod/Audio")

# Final listing
for a in unreal.EditorAssetLibrary.list_assets("/LaserRifleMod/Audio", recursive=True):
    log("LISTED " + a)
