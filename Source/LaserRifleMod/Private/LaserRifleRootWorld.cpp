#include "LaserRifleRootWorld.h"
#include "LaserRifleSubsystem.h"

ULaserRifleRootWorld::ULaserRifleRootWorld()
{
	bRootModule = true;

	// Spawn the replicated subsystem in every game world. SML reads ModSubsystems
	// during world setup.
	ModSubsystems.Add(ALaserRifleSubsystem::StaticClass());
}
