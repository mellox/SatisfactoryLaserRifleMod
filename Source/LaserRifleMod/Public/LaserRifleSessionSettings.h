#pragma once

#include "CoreMinimal.h"
#include "Settings/FGUserSettingCategory.h"
#include "LaserRifleSessionSettings.generated.h"

class USMLSessionSetting;

/**
 * SML session settings (Mod Savegame Settings menu) for the Laser Rifle.
 *
 * The subsystem is the single config surface — new toggles get added here without
 * rearchitecting. Built as Instanced subobjects of the root instance module CDO
 * (see WeaponUpgrades' pattern).
 */
UCLASS()
class LASERRIFLEMOD_API ULRSettingCategory : public UFGUserSettingCategory
{
	GENERATED_BODY()
public:
	ULRSettingCategory();
};

/** Subcategory (SML requires every setting widget to have a valid CategoryClass
 *  AND SubCategoryClass). */
UCLASS()
class LASERRIFLEMOD_API ULRSubCat_Main : public UFGUserSettingCategory
{
	GENERATED_BODY()
public:
	ULRSubCat_Main();
};

/** Second subcategory: "Advanced" -- dev/balance knobs (damage & research-tier tuning, deep heat
 *  mechanics) + WIP flags, kept out of the main list so the everyday player only sees gameplay + FX. */
UCLASS()
class LASERRIFLEMOD_API ULRSubCat_Advanced : public UFGUserSettingCategory
{
	GENERATED_BODY()
public:
	ULRSubCat_Advanced();
};

namespace LaserRifleSettings
{
	// StrId keys the subsystem reads via IFGOptionInterface.
	extern const TCHAR* const Id_DamageBase;       // float: damage gain per researched DAMAGE TIER
	extern const TCHAR* const Id_MkBump;          // float: small damage bump per Mk reached
	extern const TCHAR* const Id_HeatPerTier;     // float: +shots before overheat per Heat tier
	extern const TCHAR* const Id_CoolPerTier;     // float: cooldown speed bonus per Cooling tier
	extern const TCHAR* const Id_Exponential;      // bool : exponential vs linear curve
	extern const TCHAR* const Id_RandomComponents; // bool : FEATURE FLAG for the random kit loadout (default false)
	extern const TCHAR* const Id_FreezeAppearance; // float(int): 0 = follow research; 1..10 = lock body/beam to that Mk
	// (Removed 2026-07-05: Id_ArmsAttach was dead [the "Hold in Hands" toggle is gone], and the 8
	//  manual grip-placement settings [GripOverride/Scale/Pitch/Yaw/Roll/X/Y/Z] are superseded by the
	//  baked per-Mk placement (LevelFineRot/LevelHoldScales) + console dials (lr.HoldPitch/MuzzleD*).)
	// Feel / FX (live)
	extern const TCHAR* const Id_GlowIntensity;
	extern const TCHAR* const Id_GlowTightness;
	extern const TCHAR* const Id_BeamMin;
	extern const TCHAR* const Id_BeamIntensity; // = beam at full research
	extern const TCHAR* const Id_OverheatShots;
	extern const TCHAR* const Id_CooldownSpeed;
	extern const TCHAR* const Id_HeatBoost;
	extern const TCHAR* const Id_FireSlowdownMax;
	extern const TCHAR* const Id_OverheatPenalty;
	extern const TCHAR* const Id_SmokeAmount;
	extern const TCHAR* const Id_SmokeStartHeat;
	extern const TCHAR* const Id_SmokeOpacity;
	extern const TCHAR* const Id_SparkAmount;   // float 0..3: electrical-arc amount multiplier (Mk4+), scales with heat

	void BuildSessionSettings(UObject* Outer, TArray<TObjectPtr<USMLSessionSetting>>& OutSettings);
}
