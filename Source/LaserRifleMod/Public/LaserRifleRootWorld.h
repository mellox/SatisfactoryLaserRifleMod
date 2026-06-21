#pragma once

#include "CoreMinimal.h"
#include "Module/GameWorldModule.h"
#include "LaserRifleRootWorld.generated.h"

/**
 * Root game-world module. SML auto-discovers it (bRootModule) and spawns the
 * Laser Rifle subsystem in every game world. Will later also register the
 * Mk1-Mk10 schematics / research tree.
 */
UCLASS()
class LASERRIFLEMOD_API ULaserRifleRootWorld : public UGameWorldModule
{
	GENERATED_BODY()

public:
	ULaserRifleRootWorld();
};
