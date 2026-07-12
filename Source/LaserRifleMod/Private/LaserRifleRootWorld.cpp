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

	// WIP / PARKED (2026-07-05): the Systems tree (Heat Capacity + Cooling lines) was REMOVED -- research is
	// Mk1-10 only now (create_mam_tree.py deletes this asset). The finder is kept so re-adding the tree just
	// works, but the asset is EXPECTED to be absent right now, so the Warning below is NORMAL, not a bug.
	static ConstructorHelpers::FClassFinder<UFGResearchTree> SystemsTreeFinder(
		TEXT("/LaserRifleMod/Research/MAM_LaserRifle_Systems.MAM_LaserRifle_Systems_C"));
	if (SystemsTreeFinder.Succeeded())
	{
		mResearchTrees.Add(SystemsTreeFinder.Class);
		UE_LOG(LogLaserRifle, Log, TEXT("[LR] MAM_LaserRifle_Systems research tree registered."));
	}
	else
	{
		UE_LOG(LogLaserRifle, Log,   // parked, not an error -- Systems tree intentionally absent (Mk1-10 only)
			TEXT("[LR] MAM_LaserRifle_Systems research tree absent (parked; Mk1-10 research only)."));
	}

	UE_LOG(LogLaserRifle, Log, TEXT("[LR] RootWorld: registered %d schematics + subsystem."),
		mSchematics.Num());
}
