#pragma once

#include "CoreMinimal.h"
#include "Module/GameWorldModule.h"
#include "LaserRifleRootWorld.generated.h"

/**
 * Root game-world module: spawns the subsystem, registers the Mk1-Mk10 schematics
 * (Mk1 unlocks Recipe_LaserRifle via a constructor subobject) and the MAM tree.
 */
UCLASS()
class LASERRIFLEMOD_API ULaserRifleRootWorld : public UGameWorldModule
{
	GENERATED_BODY()

public:
	ULaserRifleRootWorld();
};
