#pragma once

#include "CoreMinimal.h"
#include "FGSchematic.h"
#include "FGSchematicCategory.h"
#include "Unlocks/FGUnlockRecipe.h"
#include "Unlocks/FGUnlockInfoOnly.h"
#include "LaserRifleSchematics.generated.h"

class UFGRecipe;
class UFGUnlock;

/**
 * Pure-C++ MAM schematics for the single "Laser Rifle" upgrade line, Mk1-Mk10.
 * Each level is its own UClass (AFGSchematicManager::IsSchematicPurchased matches
 * by exact class) so the subsystem can count the current level.
 */

// --- Concrete recipe-unlock (UFGUnlockRecipe is abstract) --------------------
UCLASS()
class LASERRIFLEMOD_API ULaserRifleUnlockRecipe : public UFGUnlockRecipe
{
	GENERATED_BODY()
public:
	/** Set the single recipe this unlock grants (mRecipes is protected on the base). */
	void LR_SetRecipe(TSubclassOf<UFGRecipe> Recipe)
	{
		mRecipes.Empty();
		if (Recipe) { mRecipes.Add(Recipe); }
	}
};

// (No custom UFGUnlockInfoOnly subclass: the MAM card only renders rows for the game's KNOWN
//  unlock Blueprint classes, so a custom C++ subclass draws nothing. We instance the STOCK
//  BP_UnlockInfoOnly_C instead — see ULaserRifleSchematic_Base::LR_AddInfoUnlock.)

// --- Category ----------------------------------------------------------------
UCLASS()
class LASERRIFLEMOD_API UCat_LaserRifle : public UFGSchematicCategory
{
	GENERATED_BODY()
public:
	UCat_LaserRifle();
};

// --- Schematic base ----------------------------------------------------------
UCLASS(Abstract)
class LASERRIFLEMOD_API ULaserRifleSchematic_Base : public UFGSchematic
{
	GENERATED_BODY()
public:
	/** Registrar-side cost injection (mCost is protected on UFGSchematic). */
	void LR_SetCost(const TArray<FItemAmount>& InCost) { mCost = InCost; }

	/** Append an unlock to this schematic (mUnlocks is protected on UFGSchematic). */
	void LR_AddUnlock(UFGUnlock* Unlock) { if (Unlock) { mUnlocks.Add(Unlock); } }

	/** Create + attach an info-only unlock carrying display text (the MAM description source).
	 *  Must be called during construction (it creates a default subobject). */
	void LR_AddInfoUnlock(const FString& Name, const FString& Desc);

	/** Fill mCost with a tier-scaled default ingredient cost (resolved via FClassFinder; skips any
	 *  item whose path fails to load, so a wrong path can never crash). Call from a constructor. */
	void LR_ApplyDefaultCost(int32 Tier);

	/** Sets the small + large schematic icon to the same texture. */
	void LR_SetIcon(class UTexture2D* Icon);

	/** Configure a stat-upgrade node (damage tier / heat / cooling). */
	void LR_InitStat(const FString& Name, const FString& Desc, int32 TechTier, float MenuPriority);

protected:
	/** Configure the inherited schematic fields. Call from the leaf constructor. */
	void LR_Init(int32 Level);
};

// --- 10 concrete schematic classes (one UCLASS per level) --------------------
UCLASS()
class LASERRIFLEMOD_API USchematic_LaserRifle_01 : public ULaserRifleSchematic_Base { GENERATED_BODY() public: USchematic_LaserRifle_01(); };
UCLASS()
class LASERRIFLEMOD_API USchematic_LaserRifle_02 : public ULaserRifleSchematic_Base { GENERATED_BODY() public: USchematic_LaserRifle_02(); };
UCLASS()
class LASERRIFLEMOD_API USchematic_LaserRifle_03 : public ULaserRifleSchematic_Base { GENERATED_BODY() public: USchematic_LaserRifle_03(); };
UCLASS()
class LASERRIFLEMOD_API USchematic_LaserRifle_04 : public ULaserRifleSchematic_Base { GENERATED_BODY() public: USchematic_LaserRifle_04(); };
UCLASS()
class LASERRIFLEMOD_API USchematic_LaserRifle_05 : public ULaserRifleSchematic_Base { GENERATED_BODY() public: USchematic_LaserRifle_05(); };
UCLASS()
class LASERRIFLEMOD_API USchematic_LaserRifle_06 : public ULaserRifleSchematic_Base { GENERATED_BODY() public: USchematic_LaserRifle_06(); };
UCLASS()
class LASERRIFLEMOD_API USchematic_LaserRifle_07 : public ULaserRifleSchematic_Base { GENERATED_BODY() public: USchematic_LaserRifle_07(); };
UCLASS()
class LASERRIFLEMOD_API USchematic_LaserRifle_08 : public ULaserRifleSchematic_Base { GENERATED_BODY() public: USchematic_LaserRifle_08(); };
UCLASS()
class LASERRIFLEMOD_API USchematic_LaserRifle_09 : public ULaserRifleSchematic_Base { GENERATED_BODY() public: USchematic_LaserRifle_09(); };
UCLASS()
class LASERRIFLEMOD_API USchematic_LaserRifle_10 : public ULaserRifleSchematic_Base { GENERATED_BODY() public: USchematic_LaserRifle_10(); };

// --- Damage tiers (Mk1 line slice: I..V; completing V gates Mk2) -------------
UCLASS() class LASERRIFLEMOD_API USchematic_LR_Dmg_01 : public ULaserRifleSchematic_Base { GENERATED_BODY() public: USchematic_LR_Dmg_01(); };
UCLASS() class LASERRIFLEMOD_API USchematic_LR_Dmg_02 : public ULaserRifleSchematic_Base { GENERATED_BODY() public: USchematic_LR_Dmg_02(); };
UCLASS() class LASERRIFLEMOD_API USchematic_LR_Dmg_03 : public ULaserRifleSchematic_Base { GENERATED_BODY() public: USchematic_LR_Dmg_03(); };
UCLASS() class LASERRIFLEMOD_API USchematic_LR_Dmg_04 : public ULaserRifleSchematic_Base { GENERATED_BODY() public: USchematic_LR_Dmg_04(); };
UCLASS() class LASERRIFLEMOD_API USchematic_LR_Dmg_05 : public ULaserRifleSchematic_Base { GENERATED_BODY() public: USchematic_LR_Dmg_05(); };

// --- Heat capacity (slice: +shots before overheat) --------------------------
UCLASS() class LASERRIFLEMOD_API USchematic_LR_Heat_01 : public ULaserRifleSchematic_Base { GENERATED_BODY() public: USchematic_LR_Heat_01(); };
UCLASS() class LASERRIFLEMOD_API USchematic_LR_Heat_02 : public ULaserRifleSchematic_Base { GENERATED_BODY() public: USchematic_LR_Heat_02(); };

// --- Cooling (slice: faster recharge) ---------------------------------------
UCLASS() class LASERRIFLEMOD_API USchematic_LR_Cool_01 : public ULaserRifleSchematic_Base { GENERATED_BODY() public: USchematic_LR_Cool_01(); };
UCLASS() class LASERRIFLEMOD_API USchematic_LR_Cool_02 : public ULaserRifleSchematic_Base { GENERATED_BODY() public: USchematic_LR_Cool_02(); };

// --- Accessors ---------------------------------------------------------------
namespace LaserRifleSchematics
{
	/** The 10 schematic classes, index 0 = Mk1 ... index 9 = Mk10. */
	void GetLevels(TArray<TSubclassOf<UFGSchematic>>& OutLevels);

	/** Same list, for mSchematics registration. */
	void GetAllSchematics(TArray<TSubclassOf<UFGSchematic>>& OutSchematics);

	/** Stat-upgrade schematic lists (for counting researched tiers). */
	void GetDamageTiers(TArray<TSubclassOf<UFGSchematic>>& Out);
	void GetHeatTiers(TArray<TSubclassOf<UFGSchematic>>& Out);
	void GetCoolTiers(TArray<TSubclassOf<UFGSchematic>>& Out);
}
