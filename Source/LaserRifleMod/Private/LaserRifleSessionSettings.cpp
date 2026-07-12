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
	const TCHAR* const Id_RandomComponents = TEXT("SS_LR_RandomComponents");
	const TCHAR* const Id_FreezeAppearance = TEXT("SS_LR_FreezeAppearance");
	// (Removed 2026-07-05: Id_ArmsAttach [dead] + the manual grip-placement cluster, superseded by
	//  baked per-Mk placement + console dials. See header note.)
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
	const TCHAR* const Id_SparkAmount    = TEXT("SS_LR_SparkAmount");
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

ULRSubCat_Advanced::ULRSubCat_Advanced()
{
	mDisplayName = NSLOCTEXT("LaserRifleMod", "SubCat_LR_Advanced", "Advanced (Balance / WIP)");
	mMenuPriority = 100.0f;   // sorts after the Main subcategory
}

namespace
{
	using namespace LaserRifleSettings;

	FSettingsWidgetLocationDescriptor MakeLocation(float MenuPriority, bool bAdvanced)
	{
		return FSettingsWidgetLocationDescriptor(
			ULRSettingCategory::StaticClass(),
			bAdvanced ? (TSubclassOf<UFGUserSettingCategory>)ULRSubCat_Advanced::StaticClass()
			          : (TSubclassOf<UFGUserSettingCategory>)ULRSubCat_Main::StaticClass(),
			/*SubOptionTo*/ nullptr,
			MenuPriority);
	}

	void FinishSetting(USMLSessionSetting* Setting, const TCHAR* StrId, const FText& Display, const FText& Tooltip,
	                   float MenuPriority, bool bAdvanced)
	{
		Setting->StrId = StrId;
		Setting->DisplayName = Display;
		Setting->ToolTip = Tooltip;
		// UpdateInstantly: live edits apply without a reload; the subsystem reads
		// them on its refresh tick. A null ApplyType is a Fatal in SML.
		Setting->ApplyType = UFGUserSettingApplyType_UpdateInstantly::StaticClass();
		Setting->WidgetsToCreate.Add(MakeLocation(MenuPriority, bAdvanced));
	}

	USMLSessionSetting* MakeSlider(UObject* Outer, const FName SubobjectName, const TCHAR* StrId,
	                               const FText& Display, const FText& Tooltip,
	                               float MinVal, float MaxVal, float DefaultVal, int32 FractionalDigits,
	                               float MenuPriority, bool bAdvanced = false)
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
		FinishSetting(Setting, StrId, Display, Tooltip, MenuPriority, bAdvanced);
		return Setting;
	}

	USMLSessionSetting* MakeCheckBox(UObject* Outer, const FName SubobjectName, const TCHAR* StrId,
	                                 const FText& Display, const FText& Tooltip, bool bDefault, float MenuPriority,
	                                 bool bAdvanced = false)
	{
		USMLSessionSetting* Setting = Outer->CreateDefaultSubobject<USMLSessionSetting>(SubobjectName);
		UFGUserSetting_CheckBox* Selector = Outer->CreateDefaultSubobject<UFGUserSetting_CheckBox>(
			FName(*(SubobjectName.ToString() + TEXT("_Sel"))));
		Selector->DefaultCheckBoxValue = bDefault;
		Setting->ValueSelector = Selector;
		FinishSetting(Setting, StrId, Display, Tooltip, MenuPriority, bAdvanced);
		return Setting;
	}
}

void LaserRifleSettings::BuildSessionSettings(UObject* Outer, TArray<TObjectPtr<USMLSessionSetting>>& OutSettings)
{
	// ================================================================================================
	// WIP / PARKED (2026-07-05): the DAMAGE-TIER and HEAT/COOLING research lines were removed from the MAM
	// tree ("only research for Mk1-10" -- see Scripts/ue/create_mam_tree.py). Their schematics + subsystem
	// tier-counts are left in place (orphaned, unresearchable), so the FOUR settings that fed them do
	// nothing. We do NOT show players configs we don't support, so they are NOT registered here at all --
	// and the code that used to read their Id_* keys is GUARDED so it never queries an unregistered option
	// (LaserRifleSubsystem::GetDamageMultiplier drops the tier term; the weapon's heat/cool reads are gated
	// behind their tier-count, which is 0). To re-add when the research returns: register the settings again
	// here (Id_DamageBase / Id_Exponential / Id_HeatPerTier / Id_CoolPerTier) and drop those guards. The
	// Id_* constants + reads stay wired so it's a small change. See also LaserRifleSchematics tier lists.
	// ================================================================================================

	// Damage: Mk Bump -- the ONLY active damage knob right now: damage = (1 + this)^Mk.
	OutSettings.Add(MakeSlider(Outer, TEXT("LR_MkBump"), Id_MkBump,
		NSLOCTEXT("LaserRifleMod", "LR_MkBump_Name", "Damage: Mk Bump"),
		NSLOCTEXT("LaserRifleMod", "LR_MkBump_Tip",
			"Extra damage for each Mk reached -- the only active damage scaler right now. Damage = (1 + this)^Mk."),
		0.0f, 0.5f, 0.05f, 2, 101.0f, /*advanced*/ true));

	// Freeze appearance: 0 = follow research level; 1..10 = lock body/beam to that
	// Mk while damage keeps scaling with the real research level (decoupled).
	OutSettings.Add(MakeSlider(Outer, TEXT("LR_FreezeAppearance"), Id_FreezeAppearance,
		NSLOCTEXT("LaserRifleMod", "LR_FreezeAppearance_Name", "Freeze Appearance at Mk"),
		NSLOCTEXT("LaserRifleMod", "LR_FreezeAppearance_Tip",
			"0 = look matches your research level. 1-10 = lock the body mesh + beam color to that Mk, while damage still scales with your real research level."),
		0.0f, 10.0f, 0.0f, 0, 2.0f));

	// "Hold in Hands" removed: the rifle is now ALWAYS held in-hand (procedural hold).
	// The old toggle didn't persist and camera-viewmodel mode is deprecated.

	// WIP / PARKED: the random kit-component loadout isn't supported for players yet, so it is NOT
	// registered in the menu. The weapon no longer reads Id_RandomComponents -- it now gates purely on the
	// lr.RandomComponents console CVar (>0 = on, default OFF), so a dev can still exercise it while players
	// never see it. To expose the toggle, re-add MakeCheckBox(... "Random Components (WIP)",
	// Id_RandomComponents, false, <priority>, /*advanced*/ true) here and AND it back into that gate.

	// (Removed 2026-07-05: the "Use Custom Grip" toggle + 7 grip-placement sliders. Held-rifle
	//  placement is now baked per-Mk (LevelFineRot / LevelHoldScales) and dialed via console vars
	//  (lr.HoldPitch/Yaw/Roll, lr.MuzzleDX/DY/DZ) -- the menu sliders were dev-tuning clutter.)

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
		NSLOCTEXT("LaserRifleMod", "LR_OverheatShots_Name", "Heat: Overheat Tolerance"),
		NSLOCTEXT("LaserRifleMod", "LR_OverheatShots_Tip",
			"Overheat is tied to CELL SIZE: at the default (6) a full cell fired rapidly overheats, at every "
			"Mk. This slider is a tolerance dial -- higher = more forgiving (overheats later), lower = "
			"overheats sooner. Heat recovers during reloads + when idle, so paced fire never overheats."),
		1.0f, 20.0f, 6.0f, 0, 20.0f));
	OutSettings.Add(MakeSlider(Outer, TEXT("LR_CooldownSpeed"), Id_CooldownSpeed,
		NSLOCTEXT("LaserRifleMod", "LR_CooldownSpeed_Name", "Heat: Cooldown Speed"),
		NSLOCTEXT("LaserRifleMod", "LR_CooldownSpeed_Tip",
			"How fast heat dissipates when you're not firing (recovers during reloads + idle). LOW Mk cools "
			"faster (recovers through its small-cell gaps); high Mk a bit slower. This slider scales the "
			"whole curve."),
		0.05f, 2.0f, 0.40f, 2, 21.0f));
	OutSettings.Add(MakeSlider(Outer, TEXT("LR_HeatBoost"), Id_HeatBoost,
		NSLOCTEXT("LaserRifleMod", "LR_HeatBoost_Name", "Heat: Glow Boost"),
		NSLOCTEXT("LaserRifleMod", "LR_HeatBoost_Tip", "How much hotter/brighter the glow gets as the rifle heats up."),
		0.0f, 5.0f, 3.0f, 1, 22.0f));
	OutSettings.Add(MakeSlider(Outer, TEXT("LR_FireSlowdownMax"), Id_FireSlowdownMax,
		NSLOCTEXT("LaserRifleMod", "LR_FireSlowdownMax_Name", "Heat: Fire Slowdown at 2x"),
		NSLOCTEXT("LaserRifleMod", "LR_FireSlowdownMax_Tip", "Past the soft limit you keep firing but slower; this is how many times slower at 2x heat (full overheat)."),
		1.0f, 10.0f, 5.0f, 1, 105.0f, /*advanced*/ true));
	OutSettings.Add(MakeSlider(Outer, TEXT("LR_OverheatPenalty"), Id_OverheatPenalty,
		NSLOCTEXT("LaserRifleMod", "LR_OverheatPenalty_Name", "Heat: Overheat Cooldown Penalty"),
		NSLOCTEXT("LaserRifleMod", "LR_OverheatPenalty_Tip", "How much slower the rifle cools when pushed to 2x (4 = four times longer cooldown at full overheat)."),
		1.0f, 8.0f, 4.0f, 1, 106.0f, /*advanced*/ true));
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
	OutSettings.Add(MakeSlider(Outer, TEXT("LR_SparkAmount"), Id_SparkAmount,
		NSLOCTEXT("LaserRifleMod", "LR_SparkAmount_Name", "FX: Electric Sparks"),
		NSLOCTEXT("LaserRifleMod", "LR_SparkAmount_Tip",
			"How many electric sparks EVERY rifle (Mk1-10) throws off WHILE FIRING (tier-coloured: amber on "
			"low Mk, violet on high). Sparks only appear while you're shooting -- this slider is just the "
			"amount. 0 = none, 1 = default, up to 3 = a lot. Also scales the branching jump-arcs and the "
			"near-overheat jump-to-ground arcs."),
		0.0f, 3.0f, 1.0f, 2, 28.0f));
}
