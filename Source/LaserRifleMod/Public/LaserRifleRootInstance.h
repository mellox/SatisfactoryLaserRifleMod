#pragma once

#include "CoreMinimal.h"
#include "Module/GameInstanceModule.h"
#include "LaserRifleRootInstance.generated.h"

/**
 * Root game-instance module. SML auto-discovers it (bRootModule) and calls
 * RegisterDefaultContent. Will later build + register the SML session-setting
 * sliders that tune the damage curve.
 */
UCLASS()
class LASERRIFLEMOD_API ULaserRifleRootInstance : public UGameInstanceModule
{
	GENERATED_BODY()

public:
	ULaserRifleRootInstance();
};
