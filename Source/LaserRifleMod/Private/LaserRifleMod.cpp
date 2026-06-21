#include "LaserRifleMod.h"

DEFINE_LOG_CATEGORY(LogLaserRifle);

void FLaserRifleModModule::StartupModule()
{
	// BUILD MARKER — bump the tag every build; verify it is embedded (UTF-16) in
	// the deployed DLL before handing off (playbook: DLL marker verification).
	UE_LOG(LogLaserRifle, Display,
		TEXT("===== LaserRifleMod BUILD 2026-06-21-scaffold-1 LOADED ====="));
}

IMPLEMENT_GAME_MODULE(FLaserRifleModModule, LaserRifleMod);
