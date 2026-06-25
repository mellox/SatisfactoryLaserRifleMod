#include "LaserRifleRootWorld.h"
#include "LaserRifleSubsystem.h"
#include "LaserRifleSchematics.h"
#include "LaserRifleMod.h"

#include "FGResearchTree.h"
#include "UObject/ConstructorHelpers.h"

ULaserRifleRootWorld::ULaserRifleRootWorld()
{
	bRootModule = true;

	// Spawn the replicated subsystem in every game world.
	ModSubsystems.Add(ALaserRifleSubsystem::StaticClass());

	// Register the 10 Mk schematics. Mk1 grants the rifle recipe via a default
	// subobject set in its constructor (see LaserRifleSchematics.cpp).
	LaserRifleSchematics::GetAllSchematics(mSchematics);

	// Register the MAM research tree (makes the schematics purchasable).
	static ConstructorHelpers::FClassFinder<UFGResearchTree> TreeFinder(
		TEXT("/LaserRifleMod/Research/MAM_LaserRifle.MAM_LaserRifle_C"));
	if (TreeFinder.Succeeded())
	{
		mResearchTrees.Add(TreeFinder.Class);
		UE_LOG(LogLaserRifle, Log, TEXT("[LR] MAM_LaserRifle research tree registered."));
	}
	else
	{
		UE_LOG(LogLaserRifle, Warning,
			TEXT("[LR] MAM_LaserRifle research tree NOT found at /LaserRifleMod/Research/."));
	}

	// Register the systems research tree (Heat Capacity + Cooling lines).
	static ConstructorHelpers::FClassFinder<UFGResearchTree> SystemsTreeFinder(
		TEXT("/LaserRifleMod/Research/MAM_LaserRifle_Systems.MAM_LaserRifle_Systems_C"));
	if (SystemsTreeFinder.Succeeded())
	{
		mResearchTrees.Add(SystemsTreeFinder.Class);
		UE_LOG(LogLaserRifle, Log, TEXT("[LR] MAM_LaserRifle_Systems research tree registered."));
	}
	else
	{
		UE_LOG(LogLaserRifle, Warning,
			TEXT("[LR] MAM_LaserRifle_Systems research tree NOT found (run create_mam_tree.py)."));
	}

	UE_LOG(LogLaserRifle, Log, TEXT("[LR] RootWorld: registered %d schematics + subsystem."),
		mSchematics.Num());
}
