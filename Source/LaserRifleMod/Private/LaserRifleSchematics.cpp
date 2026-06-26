#include "LaserRifleSchematics.h"
#include "LaserRifleMod.h"

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
	LR_ApplyDefaultCost(mTechTier);                                       // MAM ingredient cost
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

void ULaserRifleSchematic_Base::LR_ApplyDefaultCost(int32 Tier)
{
	// Tier-scaled ingredient cost for the MAM "Cost:" panel. Paths confirmed against the on-disk
	// base-game descriptors (Resource singular + item subfolder). FClassFinder runs at ctor time
	// (called from LR_Init/LR_InitStat); the .Succeeded() guard drops any path that fails to load
	// so a wrong path can never crash — it just yields a cheaper cost.
	auto Find = [](const TCHAR* Path) -> TSubclassOf<UFGItemDescriptor>
	{
		ConstructorHelpers::FClassFinder<UFGItemDescriptor> F(Path);
		return F.Succeeded() ? F.Class : nullptr;
	};
	const int32 T = FMath::Max(1, Tier);
	TSubclassOf<UFGItemDescriptor> Wire   = Find(TEXT("/Game/FactoryGame/Resource/Parts/Wire/Desc_Wire.Desc_Wire_C"));
	TSubclassOf<UFGItemDescriptor> Iron   = Find(TEXT("/Game/FactoryGame/Resource/Parts/IronPlate/Desc_IronPlate.Desc_IronPlate_C"));
	TSubclassOf<UFGItemDescriptor> Quartz = Find(TEXT("/Game/FactoryGame/Resource/Parts/QuartzCrystal/Desc_QuartzCrystal.Desc_QuartzCrystal_C"));
	TArray<FItemAmount> Cost;
	if (Wire) { Cost.Add(FItemAmount(Wire, 25 * T)); }
	if (Iron) { Cost.Add(FItemAmount(Iron, 10 * T)); }
	if (Quartz && T >= 3) { Cost.Add(FItemAmount(Quartz, 5 * (T - 2))); }
	LR_SetCost(Cost);
	UE_LOG(LogLaserRifle, Display, TEXT("[LR] MAM cost tier=%d items=%d (wire=%d iron=%d quartz=%d)"),
		T, Cost.Num(), Wire ? 1 : 0, Iron ? 1 : 0, Quartz ? 1 : 0);
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
