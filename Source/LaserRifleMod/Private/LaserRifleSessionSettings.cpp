#include "LaserRifleSessionSettings.h"
#include "LaserRifleMod.h"

#include "SessionSettings/SessionSetting.h"
#include "Settings/FGUserSetting.h"
#include "Settings/FGUserSettingApplyType.h"

namespace LaserRifleSettings
{
	const TCHAR* const Id_DamageBase       = TEXT("SS_LR_DamageBase");
	const TCHAR* const Id_MkBump           = TEXT("SS_LR_MkBump");
	const TCHAR* const Id_HeatPerTier      = TEXT("SS_LR_HeatPerTier");
	const TCHAR* const Id_CoolPerTier      = TEXT("SS_LR_CoolPerTier");
	const TCHAR* const Id_Exponential      = TEXT("SS_LR_Exponential");
	const TCHAR* const Id_FreezeAppearance = TEXT("SS_LR_FreezeAppearance");
	const TCHAR* const Id_ArmsAttach = TEXT("SS_LR_ArmsAttach");
	const TCHAR* const Id_GripOverride = TEXT("SS_LR_GripOverride");
	const TCHAR* const Id_GripScale = TEXT("SS_LR_GripScale");
	const TCHAR* const Id_GripPitch = TEXT("SS_LR_GripPitch");
	const TCHAR* const Id_GripYaw   = TEXT("SS_LR_GripYaw");
	const TCHAR* const Id_GripRoll  = TEXT("SS_LR_GripRoll");
	const TCHAR* const Id_GripX     = TEXT("SS_LR_GripX");
	const TCHAR* const Id_GripY     = TEXT("SS_LR_GripY");
	const TCHAR* const Id_GripZ     = TEXT("SS_LR_GripZ");
	const TCHAR* const Id_GlowIntensity = TEXT("SS_LR_GlowIntensity");
	const TCHAR* const Id_GlowTightness = TEXT("SS_LR_GlowTightness");
	const TCHAR* const Id_BeamMin       = TEXT("SS_LR_BeamMin");
	const TCHAR* const Id_BeamIntensity = TEXT("SS_LR_BeamIntensity");
	const TCHAR* const Id_OverheatShots = TEXT("SS_LR_OverheatShots");
	const TCHAR* const Id_CooldownSpeed = TEXT("SS_LR_CooldownSpeed");
	const TCHAR* const Id_HeatBoost     = TEXT("SS_LR_HeatBoost");
	const TCHAR* const Id_FireSlowdownMax = TEXT("SS_LR_FireSlowdownMax");
	const TCHAR* const Id_OverheatPenalty = TEXT("SS_LR_OverheatPenalty");
	const TCHAR* const Id_SmokeAmount    = TEXT("SS_LR_SmokeAmount");
	const TCHAR* const Id_SmokeStartHeat = TEXT("SS_LR_SmokeStartHeat");
	const TCHAR* const Id_SmokeOpacity   = TEXT("SS_LR_SmokeOpacity");
}

ULRSettingCategory::ULRSettingCategory()
{
	mDisplayName = NSLOCTEXT("LaserRifleMod", "Cat_LR", "Laser Rifle");
	mMenuPriority = 0.0f;
}

ULRSubCat_Main::ULRSubCat_Main()
{
	mDisplayName = NSLOCTEXT("LaserRifleMod", "SubCat_LR_Main", "Laser Rifle");
	mMenuPriority = 0.0f;
}

namespace
{
	using namespace LaserRifleSettings;

	FSettingsWidgetLocationDescriptor MakeLocation(float MenuPriority)
	{
		return FSettingsWidgetLocationDescriptor(
			ULRSettingCategory::StaticClass(),
			ULRSubCat_Main::StaticClass(),
			/*SubOptionTo*/ nullptr,
			MenuPriority);
	}

	void FinishSetting(USMLSessionSetting* Setting, const TCHAR* StrId, const FText& Display, const FText& Tooltip,
	                   float MenuPriority)
	{
		Setting->StrId = StrId;
		Setting->DisplayName = Display;
		Setting->ToolTip = Tooltip;
		// UpdateInstantly: live edits apply without a reload; the subsystem reads
		// them on its refresh tick. A null ApplyType is a Fatal in SML.
		Setting->ApplyType = UFGUserSettingApplyType_UpdateInstantly::StaticClass();
		Setting->WidgetsToCreate.Add(MakeLocation(MenuPriority));
	}

	USMLSessionSetting* MakeSlider(UObject* Outer, const FName SubobjectName, const TCHAR* StrId,
	                               const FText& Display, const FText& Tooltip,
	                               float MinVal, float MaxVal, float DefaultVal, int32 FractionalDigits,
	                               float MenuPriority)
	{
		USMLSessionSetting* Setting = Outer->CreateDefaultSubobject<USMLSessionSetting>(SubobjectName);
		UFGUserSetting_Slider* Selector = Outer->CreateDefaultSubobject<UFGUserSetting_Slider>(
			FName(*(SubobjectName.ToString() + TEXT("_Sel"))));
		Selector->MinValue = MinVal;
		Selector->MaxValue = MaxVal;
		Selector->MinDisplayValue = MinVal;
		Selector->MaxDisplayValue = MaxVal;
		Selector->MaxFractionalDigits = FractionalDigits;
		Selector->DefaultSliderValue = DefaultVal;
		Setting->ValueSelector = Selector;
		FinishSetting(Setting, StrId, Display, Tooltip, MenuPriority);
		return Setting;
	}

	USMLSessionSetting* MakeCheckBox(UObject* Outer, const FName SubobjectName, const TCHAR* StrId,
	                                 const FText& Display, const FText& Tooltip, bool bDefault, float MenuPriority)
	{
		USMLSessionSetting* Setting = Outer->CreateDefaultSubobject<USMLSessionSetting>(SubobjectName);
		UFGUserSetting_CheckBox* Selector = Outer->CreateDefaultSubobject<UFGUserSetting_CheckBox>(
			FName(*(SubobjectName.ToString() + TEXT("_Sel"))));
		Selector->DefaultCheckBoxValue = bDefault;
		Setting->ValueSelector = Selector;
		FinishSetting(Setting, StrId, Display, Tooltip, MenuPriority);
		return Setting;
	}
}

void LaserRifleSettings::BuildSessionSettings(UObject* Outer, TArray<TObjectPtr<USMLSessionSetting>>& OutSettings)
{
	// Damage base B in (1+B)^level. Default 0.16 -> x4.41 at Mk10.
	OutSettings.Add(MakeSlider(Outer, TEXT("LR_DamageBase"), Id_DamageBase,
		NSLOCTEXT("LaserRifleMod", "LR_DamageBase_Name", "Damage: per Tier"),
		NSLOCTEXT("LaserRifleMod", "LR_DamageBase_Tip",
			"Damage gained per researched damage tier (the per-Mk damage line). 0.12 ~ +12% each."),
		0.0f, 1.0f, 0.12f, 2, 0.0f));
	OutSettings.Add(MakeSlider(Outer, TEXT("LR_MkBump"), Id_MkBump,
		NSLOCTEXT("LaserRifleMod", "LR_MkBump_Name", "Damage: Mk Bump"),
		NSLOCTEXT("LaserRifleMod", "LR_MkBump_Tip", "Small extra damage for each Mk reached (the gate jump)."),
		0.0f, 0.5f, 0.05f, 2, 0.5f));
	OutSettings.Add(MakeSlider(Outer, TEXT("LR_HeatPerTier"), Id_HeatPerTier,
		NSLOCTEXT("LaserRifleMod", "LR_HeatPerTier_Name", "Research: +Shots per Heat Tier"),
		NSLOCTEXT("LaserRifleMod", "LR_HeatPerTier_Tip", "Extra shots-before-overheat granted by each Heat Capacity research."),
		0.0f, 5.0f, 1.0f, 1, 20.5f));
	OutSettings.Add(MakeSlider(Outer, TEXT("LR_CoolPerTier"), Id_CoolPerTier,
		NSLOCTEXT("LaserRifleMod", "LR_CoolPerTier_Name", "Research: Cooling per Tier"),
		NSLOCTEXT("LaserRifleMod", "LR_CoolPerTier_Tip", "Cooldown-speed bonus from each Cooling research (0.15 ~ +15% each)."),
		0.0f, 1.0f, 0.15f, 2, 21.5f));

	OutSettings.Add(MakeCheckBox(Outer, TEXT("LR_Exponential"), Id_Exponential,
		NSLOCTEXT("LaserRifleMod", "LR_Exponential_Name", "Exponential Curve"),
		NSLOCTEXT("LaserRifleMod", "LR_Exponential_Tip",
			"ON (default): damage = (1 + base)^level. OFF: linear, damage = 1 + base * level."),
		/*bDefault*/ true, 1.0f));

	// Freeze appearance: 0 = follow research level; 1..10 = lock body/beam to that
	// Mk while damage keeps scaling with the real research level (decoupled).
	OutSettings.Add(MakeSlider(Outer, TEXT("LR_FreezeAppearance"), Id_FreezeAppearance,
		NSLOCTEXT("LaserRifleMod", "LR_FreezeAppearance_Name", "Freeze Appearance at Mk"),
		NSLOCTEXT("LaserRifleMod", "LR_FreezeAppearance_Tip",
			"0 = look matches your research level. 1-10 = lock the body mesh + beam color to that Mk, while damage still scales with your real research level."),
		0.0f, 10.0f, 0.0f, 0, 2.0f));

	// "Hold in Hands" removed: the rifle is now ALWAYS held in-hand (procedural hold).
	// The old toggle didn't persist and camera-viewmodel mode is deprecated.

	// Opt-in: use the grip sliders below instead of the baked-in default placement.
	OutSettings.Add(MakeCheckBox(Outer, TEXT("LR_GripOverride"), Id_GripOverride,
		NSLOCTEXT("LaserRifleMod", "LR_GripOverride_Name", "Use Custom Grip (tuning)"),
		NSLOCTEXT("LaserRifleMod", "LR_GripOverride_Tip",
			"OFF (default): rifle uses the built-in placement. ON: use the Grip sliders below to tune it."),
		/*bDefault*/ false, 9.5f));

	// --- Live grip tuning sliders (placement of the held rifle) ---------------
	OutSettings.Add(MakeSlider(Outer, TEXT("LR_GripScale"), Id_GripScale,
		NSLOCTEXT("LaserRifleMod", "LR_GripScale_Name", "Grip: Scale"),
		NSLOCTEXT("LaserRifleMod", "LR_GripScale_Tip", "Size of the held rifle."),
		0.05f, 3.0f, 1.0f, 2, 10.0f));
	OutSettings.Add(MakeSlider(Outer, TEXT("LR_GripPitch"), Id_GripPitch,
		NSLOCTEXT("LaserRifleMod", "LR_GripPitch_Name", "Grip: Pitch"),
		NSLOCTEXT("LaserRifleMod", "LR_GripPitch_Tip", "Rotate up/down (degrees)."),
		-180.0f, 180.0f, 0.0f, 0, 11.0f));
	OutSettings.Add(MakeSlider(Outer, TEXT("LR_GripYaw"), Id_GripYaw,
		NSLOCTEXT("LaserRifleMod", "LR_GripYaw_Name", "Grip: Yaw"),
		NSLOCTEXT("LaserRifleMod", "LR_GripYaw_Tip", "Rotate left/right (degrees)."),
		-180.0f, 180.0f, 0.0f, 0, 12.0f));
	OutSettings.Add(MakeSlider(Outer, TEXT("LR_GripRoll"), Id_GripRoll,
		NSLOCTEXT("LaserRifleMod", "LR_GripRoll_Name", "Grip: Roll"),
		NSLOCTEXT("LaserRifleMod", "LR_GripRoll_Tip", "Barrel-axis roll (degrees)."),
		-180.0f, 180.0f, 0.0f, 0, 13.0f));
	OutSettings.Add(MakeSlider(Outer, TEXT("LR_GripX"), Id_GripX,
		NSLOCTEXT("LaserRifleMod", "LR_GripX_Name", "Grip: Forward/Back"),
		NSLOCTEXT("LaserRifleMod", "LR_GripX_Tip", "Forward/back from the camera (cm)."),
		-100.0f, 100.0f, 51.0f, 0, 14.0f));
	OutSettings.Add(MakeSlider(Outer, TEXT("LR_GripY"), Id_GripY,
		NSLOCTEXT("LaserRifleMod", "LR_GripY_Name", "Grip: Left/Right"),
		NSLOCTEXT("LaserRifleMod", "LR_GripY_Tip", "Left/right (cm)."),
		-100.0f, 100.0f, 10.0f, 0, 15.0f));
	OutSettings.Add(MakeSlider(Outer, TEXT("LR_GripZ"), Id_GripZ,
		NSLOCTEXT("LaserRifleMod", "LR_GripZ_Name", "Grip: Up/Down"),
		NSLOCTEXT("LaserRifleMod", "LR_GripZ_Tip", "Up/down (cm)."),
		-100.0f, 100.0f, -20.0f, 0, 16.0f));

	// --- Feel / FX sliders ----------------------------------------------------
	OutSettings.Add(MakeSlider(Outer, TEXT("LR_GlowIntensity"), Id_GlowIntensity,
		NSLOCTEXT("LaserRifleMod", "LR_GlowIntensity_Name", "FX: Glow Intensity"),
		NSLOCTEXT("LaserRifleMod", "LR_GlowIntensity_Tip", "Brightness of the rifle's energy strips."),
		0.0f, 20.0f, 6.0f, 1, 17.0f));
	OutSettings.Add(MakeSlider(Outer, TEXT("LR_GlowTightness"), Id_GlowTightness,
		NSLOCTEXT("LaserRifleMod", "LR_GlowTightness_Name", "FX: Glow Tightness"),
		NSLOCTEXT("LaserRifleMod", "LR_GlowTightness_Tip", "Higher = glow only on the brightest strips (less bleed onto the body)."),
		0.0f, 1.0f, 0.40f, 2, 18.0f));
	OutSettings.Add(MakeSlider(Outer, TEXT("LR_BeamMin"), Id_BeamMin,
		NSLOCTEXT("LaserRifleMod", "LR_BeamMin_Name", "FX: Beam Intensity (no research)"),
		NSLOCTEXT("LaserRifleMod", "LR_BeamMin_Tip", "Beam brightness with zero research."),
		0.0f, 100.0f, 10.0f, 0, 18.7f));
	OutSettings.Add(MakeSlider(Outer, TEXT("LR_BeamIntensity"), Id_BeamIntensity,
		NSLOCTEXT("LaserRifleMod", "LR_BeamIntensity_Name", "FX: Beam Intensity (full research)"),
		NSLOCTEXT("LaserRifleMod", "LR_BeamIntensity_Tip", "Beam brightness at max research; it scales up with your rifle level."),
		0.0f, 100.0f, 75.0f, 0, 19.0f));
	OutSettings.Add(MakeSlider(Outer, TEXT("LR_OverheatShots"), Id_OverheatShots,
		NSLOCTEXT("LaserRifleMod", "LR_OverheatShots_Name", "Heat: Shots to Overheat"),
		NSLOCTEXT("LaserRifleMod", "LR_OverheatShots_Tip", "Rapid shots before the rifle overheats and must cool down."),
		1.0f, 20.0f, 5.0f, 0, 20.0f));
	OutSettings.Add(MakeSlider(Outer, TEXT("LR_CooldownSpeed"), Id_CooldownSpeed,
		NSLOCTEXT("LaserRifleMod", "LR_CooldownSpeed_Name", "Heat: Cooldown Speed"),
		NSLOCTEXT("LaserRifleMod", "LR_CooldownSpeed_Tip", "How fast heat dissipates (higher = quicker recharge)."),
		0.05f, 2.0f, 0.40f, 2, 21.0f));
	OutSettings.Add(MakeSlider(Outer, TEXT("LR_HeatBoost"), Id_HeatBoost,
		NSLOCTEXT("LaserRifleMod", "LR_HeatBoost_Name", "Heat: Glow Boost"),
		NSLOCTEXT("LaserRifleMod", "LR_HeatBoost_Tip", "How much hotter/brighter the glow gets as the rifle heats up."),
		0.0f, 5.0f, 3.0f, 1, 22.0f));
	OutSettings.Add(MakeSlider(Outer, TEXT("LR_FireSlowdownMax"), Id_FireSlowdownMax,
		NSLOCTEXT("LaserRifleMod", "LR_FireSlowdownMax_Name", "Heat: Fire Slowdown at 2x"),
		NSLOCTEXT("LaserRifleMod", "LR_FireSlowdownMax_Tip", "Past the soft limit you keep firing but slower; this is how many times slower at 2x heat (full overheat)."),
		1.0f, 10.0f, 5.0f, 1, 23.0f));
	OutSettings.Add(MakeSlider(Outer, TEXT("LR_OverheatPenalty"), Id_OverheatPenalty,
		NSLOCTEXT("LaserRifleMod", "LR_OverheatPenalty_Name", "Heat: Overheat Cooldown Penalty"),
		NSLOCTEXT("LaserRifleMod", "LR_OverheatPenalty_Tip", "How much slower the rifle cools when pushed to 2x (4 = four times longer cooldown at full overheat)."),
		1.0f, 8.0f, 4.0f, 1, 24.0f));
	OutSettings.Add(MakeSlider(Outer, TEXT("LR_SmokeAmount"), Id_SmokeAmount,
		NSLOCTEXT("LaserRifleMod", "LR_SmokeAmount_Name", "FX: Smoke Amount"),
		NSLOCTEXT("LaserRifleMod", "LR_SmokeAmount_Tip", "How much steam/smoke vents from the rifle as it heats (0 = off)."),
		0.0f, 3.0f, 1.0f, 1, 25.0f));
	OutSettings.Add(MakeSlider(Outer, TEXT("LR_SmokeStartHeat"), Id_SmokeStartHeat,
		NSLOCTEXT("LaserRifleMod", "LR_SmokeStartHeat_Name", "FX: Smoke Start Heat"),
		NSLOCTEXT("LaserRifleMod", "LR_SmokeStartHeat_Tip", "Heat level where smoke begins (0..1 of the soft limit)."),
		0.0f, 1.0f, 0.30f, 2, 26.0f));
	OutSettings.Add(MakeSlider(Outer, TEXT("LR_SmokeOpacity"), Id_SmokeOpacity,
		NSLOCTEXT("LaserRifleMod", "LR_SmokeOpacity_Name", "FX: Smoke Opacity"),
		NSLOCTEXT("LaserRifleMod", "LR_SmokeOpacity_Tip", "Thickness of each puff."),
		0.0f, 1.0f, 0.55f, 2, 27.0f));
}
