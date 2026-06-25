#include "LaserRifleSubsystem.h"
#include "LaserRifleMod.h"
#include "LaserRifleSchematics.h"
#include "LaserRifleSessionSettings.h"

#include "FGSchematicManager.h"
#include "Equipment/FGAmmoType.h"
#include "DamageTypes/FGDamageType.h"
#include "SessionSettings/SessionSettingsManager.h"
#include "Engine/World.h"
#include "TimerManager.h"

ALaserRifleSubsystem::ALaserRifleSubsystem()
{
	// Populate the Mk1..Mk10 + stat-upgrade schematic class lists once.
	LaserRifleSchematics::GetLevels(RifleSchematics);
	LaserRifleSchematics::GetDamageTiers(DamageSchematics);
	LaserRifleSchematics::GetHeatTiers(HeatSchematics);
	LaserRifleSchematics::GetCoolTiers(CoolSchematics);
}

void ALaserRifleSubsystem::BeginPlay()
{
	Super::BeginPlay();

	SchematicManager = AFGSchematicManager::Get(GetWorld());

	UE_LOG(LogLaserRifle, Log, TEXT("[LR] Subsystem BeginPlay (Authority=%d, %d schematics)."),
		HasAuthority() ? 1 : 0, RifleSchematics.Num());

	// Server drives damage application; re-applies periodically so live config
	// edits (and post-research level changes) take effect without a reload.
	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(RefreshHandle, this,
			&ALaserRifleSubsystem::RefreshTick, 3.0f, /*loop*/ true, /*first delay*/ 2.0f);
	}
}

// --- Progression model -------------------------------------------------------

int32 ALaserRifleSubsystem::GetRifleLevel() const
{
	AFGSchematicManager* Mgr = SchematicManager;
	if (!Mgr)
	{
		Mgr = AFGSchematicManager::Get(GetWorld());
	}
	if (!Mgr)
	{
		return 0;
	}

	int32 Level = 0;
	for (const TSubclassOf<UFGSchematic>& Schematic : RifleSchematics)
	{
		if (Schematic && Mgr->IsSchematicPurchased(Schematic))
		{
			++Level;
		}
	}
	return Level;
}

int32 ALaserRifleSubsystem::GetVisualLevel() const
{
	// "Freeze Appearance at Mk": 0 = follow research; 1..10 = lock the look.
	const int32 Freeze = FMath::RoundToInt(GetConfigFloat(LaserRifleSettings::Id_FreezeAppearance, 0.0f));
	if (Freeze >= 1 && Freeze <= GetMaxLevel())
	{
		return Freeze;
	}
	return GetRifleLevel();
}

int32 ALaserRifleSubsystem::CountPurchased(const TArray<TSubclassOf<UFGSchematic>>& List) const
{
	AFGSchematicManager* Mgr = SchematicManager;
	if (!Mgr) { Mgr = AFGSchematicManager::Get(GetWorld()); }
	if (!Mgr) { return 0; }
	int32 N = 0;
	for (const TSubclassOf<UFGSchematic>& S : List)
	{
		if (S && Mgr->IsSchematicPurchased(S)) { ++N; }
	}
	return N;
}

int32 ALaserRifleSubsystem::GetDamageTierCount() const { return CountPurchased(DamageSchematics); }
int32 ALaserRifleSubsystem::GetHeatTierCount() const   { return CountPurchased(HeatSchematics); }
int32 ALaserRifleSubsystem::GetCoolTierCount() const   { return CountPurchased(CoolSchematics); }

float ALaserRifleSubsystem::GetDamageMultiplier() const
{
	// Damage comes mostly from the per-Mk damage line (DamagePerTier), with a
	// small bump for reaching each Mk (MkBump). Both config-tunable live.
	const int32 Mk  = GetRifleLevel();
	const int32 Dmg = GetDamageTierCount();
	const float PerTier = GetConfigFloat(LaserRifleSettings::Id_DamageBase, 0.12f);
	const float MkBump  = GetConfigFloat(LaserRifleSettings::Id_MkBump, 0.05f);
	const bool  bExp    = GetConfigBool(LaserRifleSettings::Id_Exponential, true);
	const float DmgMul  = bExp ? FMath::Pow(1.0f + PerTier, (float)Dmg) : (1.0f + PerTier * (float)Dmg);
	return DmgMul * FMath::Pow(1.0f + MkBump, (float)Mk);
}

// --- Config reads ------------------------------------------------------------

float ALaserRifleSubsystem::GetConfigFloat(const TCHAR* StrId, float DefaultValue) const
{
	const UWorld* World = GetWorld();
	const USessionSettingsManager* Mgr = World ? World->GetSubsystem<USessionSettingsManager>() : nullptr;
	if (!Mgr)
	{
		return DefaultValue;
	}
	return Mgr->GetFloatOptionValue(StrId);
}

bool ALaserRifleSubsystem::GetConfigBool(const TCHAR* StrId, bool DefaultValue) const
{
	const UWorld* World = GetWorld();
	const USessionSettingsManager* Mgr = World ? World->GetSubsystem<USessionSettingsManager>() : nullptr;
	if (!Mgr)
	{
		return DefaultValue;
	}
	return Mgr->GetBoolOptionValue(StrId);
}

// --- Damage application to our laser ammo ------------------------------------

TSubclassOf<UFGAmmoType> ALaserRifleSubsystem::ResolveAmmoClass()
{
	if (ResolvedAmmoClass)
	{
		return ResolvedAmmoClass;
	}
	if (AmmoClassPath.IsEmpty())
	{
		return nullptr;
	}
	UClass* Loaded = LoadClass<UFGAmmoType>(nullptr, *AmmoClassPath);
	if (Loaded)
	{
		ResolvedAmmoClass = Loaded;
	}
	return ResolvedAmmoClass;
}

void ALaserRifleSubsystem::RefreshTick()
{
	if (!HasAuthority())
	{
		return;
	}
	ApplyDamageScaling();
}

void ALaserRifleSubsystem::ApplyDamageScaling()
{
	TSubclassOf<UFGAmmoType> AmmoClass = ResolveAmmoClass();
	if (!AmmoClass)
	{
		return; // laser ammo asset not present yet -> no-op (safe before [asset] work)
	}

	UFGAmmoType* Cdo = AmmoClass->GetDefaultObject<UFGAmmoType>();
	if (!Cdo)
	{
		return;
	}

	// Capture the asset's authored base damages once (idempotent applies).
	// mDamageTypesOnImpact is private on UFGAmmoType -> AccessTransformers Friend.
	if (!bVanillaCaptured)
	{
		VanillaImpactDamages.Reset();
		for (const UFGDamageType* Dt : Cdo->mDamageTypesOnImpact)
		{
			VanillaImpactDamages.Add(Dt ? Dt->mDamageAmount : 0.0f);
		}
		bVanillaCaptured = true;
	}

	const float Mult = GetDamageMultiplier();
	if (FMath::IsNearlyEqual(Mult, LastAppliedMult))
	{
		return; // nothing changed since last apply
	}

	int32 i = 0;
	for (UFGDamageType* Dt : Cdo->mDamageTypesOnImpact)
	{
		if (Dt && VanillaImpactDamages.IsValidIndex(i))
		{
			Dt->mDamageAmount = VanillaImpactDamages[i] * Mult; // mDamageAmount is public
		}
		++i;
	}
	LastAppliedMult = Mult;

	UE_LOG(LogLaserRifle, Log, TEXT("[LR] Applied damage x%.2f (level %d) to %s."),
		Mult, GetRifleLevel(), *GetNameSafe(AmmoClass.Get()));
}
