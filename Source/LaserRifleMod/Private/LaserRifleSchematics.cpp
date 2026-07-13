#include "LaserRifleSchematics.h"
#include "LaserRifleMod.h"
#include "LaserRifleLog.h"

#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"
#include "FGRecipe.h"
#include "Resources/FGItemDescriptor.h"
#include "UObject/ConstructorHelpers.h"

// --- Category ----------------------------------------------------------------
UCat_LaserRifle::UCat_LaserRifle()
{
	mDisplayName = NSLOCTEXT("LaserRifleMod", "Cat_LaserRifle", "Laser Rifle");
	mMenuPriority = 0.0f;
}

// --- Schematic base ----------------------------------------------------------
void ULaserRifleSchematic_Base::LR_Init(int32 Level)
{
	mType = ESchematicType::EST_MAM;
	if (Level <= 1)
	{
		mDisplayName = FText::FromString(TEXT("Laser Rifle"));
		mDescription = FText::FromString(TEXT(
			"A directed-energy infantry weapon. Fires a focused photonic beam that needs no "
			"conventional ammunition. This research unlocks the Mk.1 frame and its fabrication recipe; "
			"later marks refine the emitter for greater damage, heat capacity, and recharge."));
	}
	else
	{
		mDisplayName = FText::FromString(FString::Printf(TEXT("Laser Rifle Mk.%d"), Level));
		mDescription = FText::FromString(FString::Printf(TEXT(
			"Upgrades the Laser Rifle to Mk.%d — a hotter, more tightly-collimated beam with a "
			"redesigned emitter housing and a brighter signature colour."), Level));
	}
	mSchematicCategory = UCat_LaserRifle::StaticClass();
	mMenuPriority = (float)Level;
	mTimeToComplete = 0.0f;
	mTechTier = FMath::Clamp(Level, 1, 9);
	mCost.Reset();
	mHiddenUntilDependenciesMet = false;
	mDependenciesBlocksSchematicAccess = false;
	LR_AddInfoUnlock(mDisplayName.ToString(), mDescription.ToString());   // MAM description source
	LR_SetCostFromRecipe(Level);                                          // MAM cost = the craft recipe (1 source)
}

void ULaserRifleSchematic_Base::LR_InitStat(const FString& Name, const FString& Desc, int32 TechTier, float MenuPriority)
{
	mType = ESchematicType::EST_MAM;
	mDisplayName = FText::FromString(Name);
	mDescription = FText::FromString(Desc);
	mSchematicCategory = UCat_LaserRifle::StaticClass();
	mMenuPriority = MenuPriority;
	mTimeToComplete = 0.0f;
	mTechTier = TechTier;
	mCost.Reset();
	mHiddenUntilDependenciesMet = false;
	mDependenciesBlocksSchematicAccess = false;
	LR_AddInfoUnlock(Name, Desc);   // MAM description source (stat nodes)
	LR_ApplyDefaultCost(TechTier);  // MAM ingredient cost
}

void ULaserRifleSchematic_Base::LR_SetIcon(UTexture2D* Icon)
{
	if (!Icon) { return; }
	mSmallSchematicIcon = Icon;
	mSchematicIcon.SetResourceObject(Icon);
	mSchematicIcon.ImageSize = FVector2D(256.0f, 256.0f);
	// Also push this per-Mk icon onto the Info-only unlock that LR_Init already created with the
	// generic Mk1 icon, so the MAM POPUP reward card matches the per-tier tree-node icon.
	for (auto& U : mUnlocks)
	{
		if (UFGUnlockInfoOnly* Info = Cast<UFGUnlockInfoOnly>(U))
		{
			Info->mUnlockIconBig = Icon;
			Info->mUnlockIconSmall = Icon;
		}
	}
}

void ULaserRifleSchematic_Base::LR_ApplyDefaultCost(int32 MkLevel)
{
	// Per-Mk MAM "Cost:" panel (what you feed the MAM to RESEARCH the node). This is a lighter
	// "research sample" that MIRRORS the craft-recipe ladder's tier-appropriate hero material for
	// each Mk (see Scripts/ue/make_mk_items.py RECIPE_LADDER + recipe-pricing-research.md), so the
	// research cost visibly progresses with the same materials as the craft cost -- iron/quartz at
	// Mk1 up to RCU/Cooling System at Mk10 -- instead of the old flat wire/iron/quartz scale.
	// FClassFinder runs at ctor time; the .Succeeded() guard drops any path that fails to load so a
	// wrong path can never crash (it just yields a cheaper cost). Item internal names that are NOT
	// obvious: Heat Sink=AluminumPlateReinforced, AI Limiter=CircuitBoardHighSpeed,
	// Supercomputer=ComputerSuper, RCU=ModularFrameLightweight.
	auto Find = [](const TCHAR* Path) -> TSubclassOf<UFGItemDescriptor>
	{
		ConstructorHelpers::FClassFinder<UFGItemDescriptor> F(Path);
		return F.Succeeded() ? F.Class : nullptr;
	};
	// MAM research cost == the WORKBENCH craft recipe's MATERIAL ingredients (the user wants the
	// MAM panel to match the workbench, minus the "consume 1x prior Mk rifle" which you can't pay
	// to research). MUST stay in sync with Scripts/ue/make_mk_items.py RECIPE_LADDER -- same items,
	// same amounts. (Kept as a literal duplicate rather than reading the recipe CDO at ctor time,
	// which is fragile w.r.t. asset load order.)
	#define LR_PART(Folder, Name) Find(TEXT("/Game/FactoryGame/Resource/Parts/" Folder "/Desc_" Name "." "Desc_" Name "_C"))
	const int32 M = FMath::Clamp(MkLevel, 1, 10);
	TArray<FItemAmount> Cost;
	auto Add = [&Cost](TSubclassOf<UFGItemDescriptor> C, int32 N){ if (C) { Cost.Add(FItemAmount(C, N)); } };
	switch (M)
	{
	case 1:  Add(LR_PART("IronPlateReinforced","IronPlateReinforced"),5); Add(LR_PART("Wire","Wire"),40); Add(LR_PART("Cable","Cable"),10); Add(LR_PART("QuartzCrystal","QuartzCrystal"),10); break;
	case 2:  Add(LR_PART("Rotor","Rotor"),3); Add(LR_PART("Cable","Cable"),20); Add(LR_PART("HighSpeedWire","HighSpeedWire"),20); Add(LR_PART("QuartzCrystal","QuartzCrystal"),15); break;
	case 3:  Add(LR_PART("CircuitBoard","CircuitBoard"),4); Add(LR_PART("HighSpeedWire","HighSpeedWire"),40); Add(LR_PART("QuartzCrystal","QuartzCrystal"),25); break;
	case 4:  Add(LR_PART("Stator","Stator"),5); Add(LR_PART("CrystalOscillator","CrystalOscillator"),3); Add(LR_PART("HighSpeedWire","HighSpeedWire"),60); break;
	case 5:  Add(LR_PART("Motor","Motor"),4); Add(LR_PART("CircuitBoard","CircuitBoard"),8); Add(LR_PART("CrystalOscillator","CrystalOscillator"),5); break;
	case 6:  Add(LR_PART("AluminumPlateReinforced","AluminumPlateReinforced"),6); Add(LR_PART("CircuitBoardHighSpeed","CircuitBoardHighSpeed"),4); Add(LR_PART("AluminumCasing","AluminumCasing"),10); break;
	case 7:  Add(LR_PART("Computer","Computer"),2); Add(LR_PART("AluminumPlateReinforced","AluminumPlateReinforced"),10); Add(LR_PART("CrystalOscillator","CrystalOscillator"),8); break;
	case 8:  Add(LR_PART("ModularFrameHeavy","ModularFrameHeavy"),3); Add(LR_PART("HighSpeedConnector","HighSpeedConnector"),8); Add(LR_PART("Computer","Computer"),3); break;
	case 9:  Add(LR_PART("CoolingSystem","CoolingSystem"),6); Add(LR_PART("ComputerSuper","ComputerSuper"),2); Add(LR_PART("CircuitBoardHighSpeed","CircuitBoardHighSpeed"),10); break;
	case 10: Add(LR_PART("ModularFrameLightweight","ModularFrameLightweight"),4); Add(LR_PART("CoolingSystem","CoolingSystem"),10); Add(LR_PART("ComputerSuper","ComputerSuper"),4); break;
	default: Add(LR_PART("Wire","Wire"),20); break;
	}
	#undef LR_PART
	LR_SetCost(Cost);
	LR_LOG(CVarLrLogGeneral, TEXT("[LR] MAM research cost Mk%d (literal fallback): %d items"), M, Cost.Num());
}

void ULaserRifleSchematic_Base::LR_SetCostFromRecipe(int32 MkLevel)
{
	// SINGLE SOURCE OF TRUTH: the MAM research node's "Cost:" == the ingredients of the SAME
	// Recipe_LaserRifle_Mk<N> asset the workbench crafts from (authored in Scripts/ue/make_mk_items.py),
	// so the two can never drift again -- including the "consume 1x prior Mk rifle" upgrade ingredient
	// (user 2026-07-05: "they should be the same recipe and it should consume the previous rifle").
	// FClassFinder force-loads the recipe class synchronously during CDO construction, so by the time
	// .Succeeded() is true its CDO's mIngredients is deserialized; GetIngredients() (line 127 FGRecipe.h)
	// is a plain CDO accessor needing no world context. If the recipe is somehow unavailable at ctor
	// time we fall back to the literal parts ladder so the node is never free.
	const int32 M = FMath::Clamp(MkLevel, 1, 10);
	const FString Path = FString::Printf(
		TEXT("/LaserRifleMod/Recipes/Recipe_LaserRifle_Mk%d.Recipe_LaserRifle_Mk%d_C"), M, M);
	ConstructorHelpers::FClassFinder<UFGRecipe> RecipeFinder(*Path);
	if (RecipeFinder.Succeeded())
	{
		if (const UFGRecipe* RecipeCDO = GetDefault<UFGRecipe>(RecipeFinder.Class))
		{
			const TArray<FItemAmount>& Ings = RecipeCDO->GetIngredients();
			if (Ings.Num() > 0)
			{
				LR_SetCost(Ings);
				LR_LOG(CVarLrLogGeneral,
					TEXT("[LR] MAM cost Mk%d <- Recipe_LaserRifle_Mk%d (%d ingredients, matches workbench)"),
					M, M, Ings.Num());
				return;
			}
		}
	}
	UE_LOG(LogLaserRifle, Warning,
		TEXT("[LR] MAM cost Mk%d: recipe %s unavailable at ctor -> literal-ladder fallback"), M, *Path);
	LR_ApplyDefaultCost(M);
}

void ULaserRifleSchematic_Base::LR_AddInfoUnlock(const FString& Name, const FString& Desc)
{
	// The MAM card sources its description from the schematic's UNLOCKS, and only renders rows for
	// the game's KNOWN unlock Blueprint classes — a custom C++ subclass draws NOTHING (that was the
	// blank). So instance the STOCK BP_UnlockInfoOnly (exactly what the shipped ExampleMod does) as a
	// default subobject of this CDO so it serializes/cooks, and fill its text + icon.
	static ConstructorHelpers::FClassFinder<UFGUnlockInfoOnly> InfoBP(
		TEXT("/Game/FactoryGame/Unlocks/BP_UnlockInfoOnly.BP_UnlockInfoOnly_C"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> InfoIcon(
		TEXT("/LaserRifleMod/Equipment/LaserRifle/Icons/T_LaserRifle_Icon_Mk1.T_LaserRifle_Icon_Mk1"));
	if (!InfoBP.Succeeded()) { return; }

	UFGUnlockInfoOnly* Info = Cast<UFGUnlockInfoOnly>(CreateDefaultSubobject(
		TEXT("InfoUnlock"), UFGUnlockInfoOnly::StaticClass(), InfoBP.Class, /*bIsRequired*/ true, /*bIsTransient*/ false));
	if (Info)
	{
		// Protected on UFGUnlockInfoOnly; reachable via the AccessTransformers Friend grant.
		Info->mUnlockName = FText::FromString(Name);
		Info->mUnlockDescription = FText::FromString(Desc);
		if (InfoIcon.Succeeded()) { Info->mUnlockIconBig = InfoIcon.Object; Info->mUnlockIconSmall = InfoIcon.Object; }
		mUnlocks.Add(Info);
	}
}

// --- Mk1: also unlocks the rifle recipe (proper default subobject, NOT a -----
// runtime CDO mutation). Resolving the recipe via FClassFinder at construction
// makes the unlock a real serialized subobject of the schematic CDO.
USchematic_LaserRifle_01::USchematic_LaserRifle_01()
{
	LR_Init(1);

	static ConstructorHelpers::FObjectFinder<UTexture2D> IconF(
		TEXT("/LaserRifleMod/Equipment/LaserRifle/Icons/T_LaserRifle_Icon_Mk1.T_LaserRifle_Icon_Mk1"));
	if (IconF.Succeeded()) { LR_SetIcon(IconF.Object); }

	static ConstructorHelpers::FClassFinder<UFGRecipe> RecipeFinder(
		TEXT("/LaserRifleMod/Recipes/Recipe_LaserRifle_Mk1.Recipe_LaserRifle_Mk1_C"));
	if (RecipeFinder.Succeeded())
	{
		ULaserRifleUnlockRecipe* Unlock =
			CreateDefaultSubobject<ULaserRifleUnlockRecipe>(TEXT("RecipeUnlock"));
		Unlock->LR_SetRecipe(RecipeFinder.Class);
		mUnlocks.Add(Unlock);
	}

	// Also unlock the ENERGY CELL fuel recipe here, so researching the Mk1 rifle grants its craftable fuel at
	// the SAME time. This fixes the early-rifle-with-no-fuel gap: the rifle burns Energy Cells to reload, and
	// before this the reload pulled a base-game Battery (unlocks ~Tier 8) -> a fresh Mk1 bricked once its
	// starting charge ran out. Guarded FClassFinder: if Recipe_LR_EnergyCell isn't cooked yet (e.g. the first
	// build before the Python author step) this simply adds no unlock -- it never crashes. See [[laserrifle-energycell]].
	static ConstructorHelpers::FClassFinder<UFGRecipe> CellRecipeFinder(
		TEXT("/LaserRifleMod/Recipes/Recipe_LR_EnergyCell.Recipe_LR_EnergyCell_C"));
	if (CellRecipeFinder.Succeeded())
	{
		ULaserRifleUnlockRecipe* CellUnlock =
			CreateDefaultSubobject<ULaserRifleUnlockRecipe>(TEXT("CellRecipeUnlock"));
		CellUnlock->LR_SetRecipe(CellRecipeFinder.Class);
		mUnlocks.Add(CellUnlock);
	}
}

// --- Mk2..Mk10 (no recipe unlock; pure level markers) ------------------------
#define LR_SCHEMATIC_CTOR(ClassName, Level) \
	ClassName::ClassName() { \
		LR_Init(Level); \
		static ConstructorHelpers::FObjectFinder<UTexture2D> IconF( \
			TEXT("/LaserRifleMod/Equipment/LaserRifle/Icons/T_LaserRifle_Icon_Mk" #Level ".T_LaserRifle_Icon_Mk" #Level)); \
		if (IconF.Succeeded()) { LR_SetIcon(IconF.Object); } \
		/* Separate-item design: this Mk schematic unlocks its OWN craftable rifle recipe. */ \
		static ConstructorHelpers::FClassFinder<UFGRecipe> RecipeFinder( \
			TEXT("/LaserRifleMod/Recipes/Recipe_LaserRifle_Mk" #Level ".Recipe_LaserRifle_Mk" #Level "_C")); \
		if (RecipeFinder.Succeeded()) { \
			ULaserRifleUnlockRecipe* Unlock = CreateDefaultSubobject<ULaserRifleUnlockRecipe>(TEXT("RecipeUnlock")); \
			Unlock->LR_SetRecipe(RecipeFinder.Class); \
			mUnlocks.Add(Unlock); \
		} \
	}

LR_SCHEMATIC_CTOR(USchematic_LaserRifle_02, 2)
LR_SCHEMATIC_CTOR(USchematic_LaserRifle_03, 3)
LR_SCHEMATIC_CTOR(USchematic_LaserRifle_04, 4)
LR_SCHEMATIC_CTOR(USchematic_LaserRifle_05, 5)
LR_SCHEMATIC_CTOR(USchematic_LaserRifle_06, 6)
LR_SCHEMATIC_CTOR(USchematic_LaserRifle_07, 7)
LR_SCHEMATIC_CTOR(USchematic_LaserRifle_08, 8)
LR_SCHEMATIC_CTOR(USchematic_LaserRifle_09, 9)
LR_SCHEMATIC_CTOR(USchematic_LaserRifle_10, 10)

#undef LR_SCHEMATIC_CTOR

// ================================================================================================
// WIP / PARKED (2026-07-05): these Damage / Heat / Cooling stat-upgrade schematics are ORPHANED. Their
// research NODES were removed from the MAM tree ("only research for Mk1-10" -- see create_mam_tree.py),
// so they are defined + registered but UNRESEARCHABLE, and the subsystem tier-counts that read them stay
// 0 (damage scales purely by Mk). Left in place so the feature is a re-add away: to re-enable, add their
// nodes back in Scripts/ue/create_mam_tree.py and rebuild the tree assets. Their session settings are
// tagged "(inactive)" until then. DO NOT assume these do anything at runtime right now.
// ================================================================================================
#define LR_STAT_CTOR(ClassName, Name, Desc, Tier, Prio) \
	ClassName::ClassName() { \
		LR_InitStat(TEXT(Name), TEXT(Desc), Tier, Prio); \
		static ConstructorHelpers::FObjectFinder<UTexture2D> IconF( \
			TEXT("/LaserRifleMod/Equipment/LaserRifle/Icons/T_LaserRifle_Icon_Mk1.T_LaserRifle_Icon_Mk1")); \
		if (IconF.Succeeded()) { LR_SetIcon(IconF.Object); } \
	}

LR_STAT_CTOR(USchematic_LR_Dmg_01, "Laser Rifle - Damage I",  "Increase laser rifle damage. (Mk1 line, tier 1/5)", 1, 1.1f)
LR_STAT_CTOR(USchematic_LR_Dmg_02, "Laser Rifle - Damage II", "Increase laser rifle damage. (Mk1 line, tier 2/5)", 1, 1.2f)
LR_STAT_CTOR(USchematic_LR_Dmg_03, "Laser Rifle - Damage III","Increase laser rifle damage. (Mk1 line, tier 3/5)", 1, 1.3f)
LR_STAT_CTOR(USchematic_LR_Dmg_04, "Laser Rifle - Damage IV", "Increase laser rifle damage. (Mk1 line, tier 4/5)", 1, 1.4f)
LR_STAT_CTOR(USchematic_LR_Dmg_05, "Laser Rifle - Damage V",  "Increase laser rifle damage. Completes the Mk1 damage line and unlocks Mk2.", 1, 1.5f)
LR_STAT_CTOR(USchematic_LR_Heat_01, "Laser Rifle - Heat Capacity I",  "More shots before overheating.", 1, 2.1f)
LR_STAT_CTOR(USchematic_LR_Heat_02, "Laser Rifle - Heat Capacity II", "More shots before overheating.", 2, 2.2f)
LR_STAT_CTOR(USchematic_LR_Cool_01, "Laser Rifle - Cooling I",  "Faster cooldown / recharge.", 1, 3.1f)
LR_STAT_CTOR(USchematic_LR_Cool_02, "Laser Rifle - Cooling II", "Faster cooldown / recharge.", 2, 3.2f)

#undef LR_STAT_CTOR

// --- Accessors ---------------------------------------------------------------
namespace LaserRifleSchematics
{
	void GetLevels(TArray<TSubclassOf<UFGSchematic>>& OutLevels)
	{
		OutLevels.Reset();
		OutLevels.Add(USchematic_LaserRifle_01::StaticClass());
		OutLevels.Add(USchematic_LaserRifle_02::StaticClass());
		OutLevels.Add(USchematic_LaserRifle_03::StaticClass());
		OutLevels.Add(USchematic_LaserRifle_04::StaticClass());
		OutLevels.Add(USchematic_LaserRifle_05::StaticClass());
		OutLevels.Add(USchematic_LaserRifle_06::StaticClass());
		OutLevels.Add(USchematic_LaserRifle_07::StaticClass());
		OutLevels.Add(USchematic_LaserRifle_08::StaticClass());
		OutLevels.Add(USchematic_LaserRifle_09::StaticClass());
		OutLevels.Add(USchematic_LaserRifle_10::StaticClass());
	}

	void GetDamageTiers(TArray<TSubclassOf<UFGSchematic>>& Out)
	{
		Out.Reset();
		Out.Add(USchematic_LR_Dmg_01::StaticClass());
		Out.Add(USchematic_LR_Dmg_02::StaticClass());
		Out.Add(USchematic_LR_Dmg_03::StaticClass());
		Out.Add(USchematic_LR_Dmg_04::StaticClass());
		Out.Add(USchematic_LR_Dmg_05::StaticClass());
	}
	void GetHeatTiers(TArray<TSubclassOf<UFGSchematic>>& Out)
	{
		Out.Reset();
		Out.Add(USchematic_LR_Heat_01::StaticClass());
		Out.Add(USchematic_LR_Heat_02::StaticClass());
	}
	void GetCoolTiers(TArray<TSubclassOf<UFGSchematic>>& Out)
	{
		Out.Reset();
		Out.Add(USchematic_LR_Cool_01::StaticClass());
		Out.Add(USchematic_LR_Cool_02::StaticClass());
	}

	void GetAllSchematics(TArray<TSubclassOf<UFGSchematic>>& OutSchematics)
	{
		GetLevels(OutSchematics);
		TArray<TSubclassOf<UFGSchematic>> Tmp;
		GetDamageTiers(Tmp); OutSchematics.Append(Tmp);
		GetHeatTiers(Tmp);   OutSchematics.Append(Tmp);
		GetCoolTiers(Tmp);   OutSchematics.Append(Tmp);
	}
}
