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

namespace LaserRifleSettings
{
	// StrId keys the subsystem reads via IFGOptionInterface.
	extern const TCHAR* const Id_DamageBase;       // float: damage gain per researched DAMAGE TIER
	extern const TCHAR* const Id_MkBump;          // float: small damage bump per Mk reached
	extern const TCHAR* const Id_HeatPerTier;     // float: +shots before overheat per Heat tier
	extern const TCHAR* const Id_CoolPerTier;     // float: cooldown speed bonus per Cooling tier
	extern const TCHAR* const Id_Exponential;      // bool : exponential vs linear curve
	extern const TCHAR* const Id_FreezeAppearance; // float(int): 0 = follow research; 1..10 = lock body/beam to that Mk
	// Hold mode: false = floating viewmodel (default); true = attach to hands (experimental).
	extern const TCHAR* const Id_ArmsAttach;
	// Use custom grip sliders instead of the baked-in default placement.
	extern const TCHAR* const Id_GripOverride;
	// Live grip tuning (viewmodel placement). Baked once dialed in.
	extern const TCHAR* const Id_GripScale;
	extern const TCHAR* const Id_GripPitch;
	extern const TCHAR* const Id_GripYaw;
	extern const TCHAR* const Id_GripRoll;
	extern const TCHAR* const Id_GripX;
	extern const TCHAR* const Id_GripY;
	extern const TCHAR* const Id_GripZ;
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

	void BuildSessionSettings(UObject* Outer, TArray<TObjectPtr<USMLSessionSetting>>& OutSettings);
}
