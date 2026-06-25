#pragma once

#include "CoreMinimal.h"
#include "Subsystem/ModSubsystem.h"
#include "Templates/SubclassOf.h"
#include "LaserRifleSubsystem.generated.h"

class UFGSchematic;
class AFGSchematicManager;

/**
 * Replicated mod subsystem for the Laser Rifle.
 *
 * Owns the progression model and is the single config surface:
 *   - GetRifleLevel()       : how many Mk schematics are purchased (0..10).
 *   - GetDamageMultiplier() : the curve over the research level (config-tuned).
 *   - GetVisualLevel()      : which Mk look to show (research level, OR a config
 *                             "freeze appearance" override) -- INDEPENDENT of
 *                             damage, so a future toggle can lock the look while
 *                             damage keeps scaling.
 * The weapon Blueprint reads these getters to swap body mesh + beam color.
 *
 * On the authority it also applies the damage multiplier to our laser ammo type's
 * impact damage (WeaponUpgrades pattern), capturing the asset's base damage once
 * so repeated applies are idempotent.
 */
UCLASS()
class LASERRIFLEMOD_API ALaserRifleSubsystem : public AModSubsystem
{
	GENERATED_BODY()

public:
	ALaserRifleSubsystem();

	virtual void BeginPlay() override;

	/** Highest Mk currently researched (count of purchased Rifle schematics, 0..10). */
	UFUNCTION( BlueprintCallable, BlueprintPure, Category = "LaserRifle" )
	int32 GetRifleLevel() const;

	/** Mk level used for VISUALS. Equals the research level unless the
	 *  "Freeze Appearance at Mk" config (1..10) overrides it. */
	UFUNCTION( BlueprintCallable, BlueprintPure, Category = "LaserRifle" )
	int32 GetVisualLevel() const;

	/** Damage multiplier for the current research level (independent of visuals). */
	UFUNCTION( BlueprintCallable, BlueprintPure, Category = "LaserRifle" )
	float GetDamageMultiplier() const;

	/** Max Mk (10). */
	UFUNCTION( BlueprintCallable, BlueprintPure, Category = "LaserRifle" )
	int32 GetMaxLevel() const { return 10; }

	/** Researched stat-upgrade tiers (counts of purchased upgrade schematics). */
	UFUNCTION( BlueprintCallable, BlueprintPure, Category = "LaserRifle" )
	int32 GetDamageTierCount() const;
	UFUNCTION( BlueprintCallable, BlueprintPure, Category = "LaserRifle" )
	int32 GetHeatTierCount() const;
	UFUNCTION( BlueprintCallable, BlueprintPure, Category = "LaserRifle" )
	int32 GetCoolTierCount() const;

protected:
	/** Path to our laser ammo descriptor (an [asset] UFGAmmoTypeLaser authored in
	 *  the editor). Damage scaling no-ops until this resolves. */
	UPROPERTY( EditDefaultsOnly, Category = "LaserRifle" )
	FString AmmoClassPath = TEXT("/LaserRifleMod/Equipment/LaserRifle/Ammo_LaserRifle.Ammo_LaserRifle_C");

	/** The 10 Mk schematic classes, index 0 = Mk1 ... 9 = Mk10. */
	UPROPERTY()
	TArray<TSubclassOf<UFGSchematic>> RifleSchematics;

	/** Stat-upgrade schematic lists (damage tiers, heat capacity, cooling). */
	UPROPERTY()
	TArray<TSubclassOf<UFGSchematic>> DamageSchematics;
	UPROPERTY()
	TArray<TSubclassOf<UFGSchematic>> HeatSchematics;
	UPROPERTY()
	TArray<TSubclassOf<UFGSchematic>> CoolSchematics;

private:
	float GetConfigFloat(const TCHAR* StrId, float DefaultValue) const;
	bool  GetConfigBool(const TCHAR* StrId, bool DefaultValue) const;
	int32 CountPurchased(const TArray<TSubclassOf<UFGSchematic>>& List) const;

	void RefreshTick();
	void ApplyDamageScaling();
	TSubclassOf<class UFGAmmoType> ResolveAmmoClass();

	UPROPERTY()
	TObjectPtr<AFGSchematicManager> SchematicManager = nullptr;

	UPROPERTY()
	TSubclassOf<class UFGAmmoType> ResolvedAmmoClass = nullptr;

	/** Base (asset-authored) impact damages, captured once so applies are idempotent. */
	TArray<float> VanillaImpactDamages;
	bool bVanillaCaptured = false;

	FTimerHandle RefreshHandle;

	/** Last applied multiplier, to skip redundant CDO writes. */
	float LastAppliedMult = -1.0f;
};
