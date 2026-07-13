#include "LaserRifleMod.h"

DEFINE_LOG_CATEGORY(LogLaserRifle);

void FLaserRifleModModule::StartupModule()
{
	// BUILD MARKER -- bump the tag every build; verify it is embedded (UTF-16)
	// in the deployed Shipping DLL before handing off (playbook: DLL marker check).
	UE_LOG(LogLaserRifle, Display,
		TEXT("===== LaserRifleMod BUILD 2026-07-13-cell-machine LOADED ====="));
}

IMPLEMENT_GAME_MODULE(FLaserRifleModModule, LaserRifleMod);
