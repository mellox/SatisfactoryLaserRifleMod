#pragma once

#include "CoreMinimal.h"
#include "Subsystem/ModSubsystem.h"
#include "LaserRifleSubsystem.generated.h"

/**
 * Replicated mod subsystem for the Laser Rifle.
 *
 * Scaffold stage: empty shell that SML spawns per world via the root world module.
 * Will later track the purchased Mk1-Mk10 research level and expose the
 * replicated per-level damage / beam / mesh values the rifle reads.
 */
UCLASS()
class LASERRIFLEMOD_API ALaserRifleSubsystem : public AModSubsystem
{
	GENERATED_BODY()

public:
	ALaserRifleSubsystem();
};
