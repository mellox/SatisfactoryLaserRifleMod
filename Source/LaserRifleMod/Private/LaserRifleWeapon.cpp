#include "LaserRifleWeapon.h"
#include "LaserRifleSubsystem.h"
#include "LaserRifleMod.h"
#include "LaserRifleLog.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/CameraComponent.h"
#include "Subsystem/SubsystemActorManager.h"
#include "FGCharacterPlayer.h"
#include "FGInventoryComponent.h"
#include "Resources/FGItemDescriptor.h"
#include "GameFramework/PlayerController.h"
#include "FGHUD.h"
#include "LaserRifleCrosshair.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Pawn.h"
#include "Components/SkeletalMeshComponent.h"
#include "Equipment/FGEquipment.h"
#include "CharacterAnimationTypes.h"
#include "SessionSettings/SessionSettingsManager.h"
#include "LaserRifleSessionSettings.h"
#include "GameFramework/DamageType.h"
#include "DamageTypes/FGDamageType.h"   // UFGDamageType -- required so FG actors' TakeDamage cast doesn't crash
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "AkAudioEvent.h"
#include "AkGameplayStatics.h"
#include "TimerManager.h"
#include "InputCoreTypes.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Math/RandomStream.h"

// --- Dev console command: give the local player one of each Mk1-10 laser rifle, so every tier
//     can be equipped for muzzle/visual tuning without researching + crafting the whole ladder.
//     In-game console: lr.GiveRifles  (behind the console -> dev/testing only). ---
static void LR_GiveAllRifles(UWorld* World)
{
	if (!World) { return; }
	AFGCharacterPlayer* Ch = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerCharacter(World, 0));
	if (!Ch)  { UE_LOG(LogLaserRifle, Warning, TEXT("[LR] GiveRifles: no local player character.")); return; }
	UFGInventoryComponent* Inv = Ch->GetInventory();
	if (!Inv) { UE_LOG(LogLaserRifle, Warning, TEXT("[LR] GiveRifles: player has no inventory.")); return; }
	int32 Given = 0;
	for (int32 M = 1; M <= 10; ++M)
	{
		const FString Path = FString::Printf(
			TEXT("/LaserRifleMod/Equipment/LaserRifle/Desc_LaserRifle_Mk%d.Desc_LaserRifle_Mk%d_C"), M, M);
		UClass* DescCls = LoadClass<UFGItemDescriptor>(nullptr, *Path);
		if (!DescCls)
		{
			UE_LOG(LogLaserRifle, Warning, TEXT("[LR] GiveRifles: Mk%d descriptor not found (%s)"), M, *Path);
			continue;
		}
		const int32 Added = Inv->AddStack(FInventoryStack(1, TSubclassOf<UFGItemDescriptor>(DescCls)), true);
		if (Added > 0) { ++Given; }
		UE_LOG(LogLaserRifle, Display, TEXT("[LR] GiveRifles: Mk%d added=%d"), M, Added);
	}
	UE_LOG(LogLaserRifle, Display, TEXT("[LR] GiveRifles: gave %d/10 Mk rifles."), Given);
}
static FAutoConsoleCommandWithWorld GCmdLRGiveRifles(
	TEXT("lr.GiveRifles"),
	TEXT("Dev: add one of each Mk1-10 Laser Rifle to the local player's inventory (for tuning/testing)."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&LR_GiveAllRifles));

// --- Rig driver console vars: live-tunable in-game so ONE build tests many behaviours. ---
static TAutoConsoleVariable<int32> CVarRigEnable(TEXT("lr.RigEnable"), 0,
	TEXT("LaserRifle: use the OLD rigged skeletal pilot body on Mk1 (1=on) instead of the shipped static ")
	TEXT("frankenrifle body + dangling-battery component (0=default). Kept for A/B; the Mk1 ships static."));
static TAutoConsoleVariable<int32> CVarRigReactive(TEXT("lr.RigBatteryReactive"), 1,
	TEXT("LaserRifle: dangling battery reacts to rifle motion/recoil (1) vs gentle idle sway only (0)."));
static TAutoConsoleVariable<int32> CVarRigFloppy(TEXT("lr.RigWireFloppy"), 1,
	TEXT("LaserRifle: wires bend as a floppy multi-bone chain (1) vs rigid pendant (0)."));
static TAutoConsoleVariable<float> CVarRigAmplitude(TEXT("lr.RigAmplitude"), 1.0f,
	TEXT("LaserRifle: global multiplier on procedural bone swing amplitude."));
static TAutoConsoleVariable<float> CVarRigDamping(TEXT("lr.RigDamping"), 1.0f,
	TEXT("LaserRifle: multiplier on spring damping (higher = settles faster / less floppy)."));
static TAutoConsoleVariable<int32> CVarRigDebug(TEXT("lr.RigDebug"), 0,
	TEXT("LaserRifle: 1 = verbose [LR] rig driver logs (component accel + per-joint angles)."));
static TAutoConsoleVariable<float> CVarRigYaw(TEXT("lr.RigYaw"), 180.0f,
	TEXT("LaserRifle: extra yaw (deg, about the rig's up axis) so the rigged mesh's barrel points forward."));
static TAutoConsoleVariable<float> CVarRigPitch(TEXT("lr.RigPitch"), 0.0f,
	TEXT("LaserRifle: extra pitch (deg) fine-tune for the rigged mesh orientation."));
static TAutoConsoleVariable<float> CVarRigRoll(TEXT("lr.RigRoll"), 0.0f,
	TEXT("LaserRifle: extra roll (deg) fine-tune for the rigged mesh orientation."));
// --- Dangling battery (Mk1) live tuning: nudge the mount in BODY-LOCAL cm, scale + swing, then bake. ---
// PARKED 2026-07-05 (user: "get rid of the battery on the mk1, we'll do it a different time"). The whole
// dangling-battery + cable feature stays intact but OFF by default; flip this to 1 (or type lr.BatteryEnable 1)
// to bring it back. See [[laserrifle-battery-energy-vision]].
static TAutoConsoleVariable<int32> CVarBatteryEnable(TEXT("lr.BatteryEnable"), 0,
	TEXT("LaserRifle: Mk1 dangling Satisfactory battery + cables (1=on, 0=default/PARKED 2026-07-05 per user)."));
static TAutoConsoleVariable<float> CVarBatteryX(TEXT("lr.BatteryX"), 0.f,
	TEXT("LaserRifle: Mk1 dangling-battery mount nudge, body-local X in cm (+forward)."));
static TAutoConsoleVariable<float> CVarBatteryY(TEXT("lr.BatteryY"), 0.f,
	TEXT("LaserRifle: Mk1 dangling-battery mount nudge, body-local Y in cm (+right)."));
static TAutoConsoleVariable<float> CVarBatteryZ(TEXT("lr.BatteryZ"), 0.f,
	TEXT("LaserRifle: Mk1 dangling-battery mount nudge, body-local Z in cm (+up)."));
static TAutoConsoleVariable<float> CVarBatteryScale(TEXT("lr.BatteryScale"), 1.f,
	TEXT("LaserRifle: Mk1 dangling-battery size multiplier (on top of the auto ~25cm normalize)."));
static TAutoConsoleVariable<float> CVarBatterySwing(TEXT("lr.BatterySwing"), 1.f,
	TEXT("LaserRifle: Mk1 dangling-battery swing amplitude multiplier (0 = rigid)."));
static TAutoConsoleVariable<int32> CVarBatteryDebug(TEXT("lr.BatteryDebug"), 0,
	TEXT("LaserRifle: 1 = verbose [LR] dangling-battery logs (insert/show/swing/mount)."));
static TAutoConsoleVariable<int32> CVarRandomComponents(TEXT("lr.RandomComponents"), 0,
	TEXT("LaserRifle: any change to this value re-rolls 6 random kit components onto the held ")
	TEXT("rifle's static BodyMesh (test command -- type e.g. 'lr.RandomComponents 1', then 2, 3... ")
	TEXT("to re-roll again; no-ops if KitComponentPool is empty)."));
// Live beam-origin (muzzle) tuning. Nudge the CURRENTLY-equipped rifle's muzzle in MESH-LOCAL cm
// (+X fwd, +Y right, +Z up) and fire to watch the beam origin move -- lets the emitter be placed
// exactly, no rebuild, then baked into LevelMuzzleOffsets[Mk]. The [LR] BEAMDIAG line's muzzleLocal
// already includes the nudge, so read the final value off the log. Default 0 = use the baked muzzle.
static TAutoConsoleVariable<float> CVarMuzzleDX(TEXT("lr.MuzzleDX"), 0.0f, TEXT("Muzzle nudge +X (forward) mesh-local cm"));
static TAutoConsoleVariable<float> CVarMuzzleDY(TEXT("lr.MuzzleDY"), 0.0f, TEXT("Muzzle nudge +Y (right) mesh-local cm"));
static TAutoConsoleVariable<float> CVarMuzzleDZ(TEXT("lr.MuzzleDZ"), 0.0f, TEXT("Muzzle nudge +Z (up) mesh-local cm"));
// Live HELD-orientation tuning. Tilt the CURRENTLY-equipped rifle so its sculpted barrel lines up
// with the crosshair (some bodies' visible barrel isn't along the mesh +X the game aims -> the gun
// reads as pointing below the crosshair). Added on top of the baked LevelFineRot[Mk]; find the value
// live, then report it to bake. +Pitch tilts the muzzle UP. Degrees. Default 0.
static TAutoConsoleVariable<float> CVarHoldPitch(TEXT("lr.HoldPitch"), 0.0f, TEXT("Held-rifle pitch tweak deg (+ = muzzle up)"));
static TAutoConsoleVariable<float> CVarHoldYaw(TEXT("lr.HoldYaw"), 0.0f, TEXT("Held-rifle yaw tweak deg"));
static TAutoConsoleVariable<float> CVarHoldRoll(TEXT("lr.HoldRoll"), 0.0f, TEXT("Held-rifle roll tweak deg"));

// Laser fuel-cell ammo descriptor. Magazine size MUST match ShotsPerCell (30) so the
// native ammo HUD shows "<shots left> / 30". Display-only: firing stays custom.
ULaserRifleAmmo::ULaserRifleAmmo()
{
	mMagazineSize = 30;   // protected in UFGAmmoType; matches ALaserRifleWeapon::ShotsPerCell
}

// Laser damage type (fixes the AFGSporeFlower TakeDamage crash -- see the FireLaser ApplyPointDamage call).
// UFGDamageType's ctor zeroes the physics impulses the base UDamageType sets to 800, so restore them (+ let it
// hurt destructibles) to keep the same knockback / destructible feel the laser had before the fix.
ULaserRifleDamageType::ULaserRifleDamageType(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DamageImpulse             = 800.f;   // base UDamageType default; UFGDamageType's ctor zeroes it
	DestructibleImpulse       = 800.f;
	mShouldDamageDestructible = true;    // keep destructibles damageable (base UDamageType had no such gate)
}

ALaserRifleWeapon::ALaserRifleWeapon()
{
	PrimaryActorTick.bCanEverTick = true;
	// Update the held-rifle transform AFTER the camera finalizes for the frame, so it
	// doesn't lag the view by a frame when turning (the classic viewmodel-stutter fix).
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	if (RootComponent) { BodyMesh->SetupAttachment(RootComponent); }
	else { RootComponent = BodyMesh; }
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetCastShadow(false);
	BodyMesh->SetVisibility(true);

	// Random kit-component loadout (lr.RandomComponents test): 6 slots. [1..5] use ordinary
	// RELATIVE attachment to BodyMesh (ride BodyMesh's transform automatically like any normal
	// child, static-path only). [0] is special-cased ABSOLUTE (like SmokeComps/BeamMesh) since
	// it doubles as the "dangle" test slot -- DriveRigBones sets its world transform directly
	// from the pend_2 bone pose when the rig is active, independent of BodyMesh's (hidden)
	// transform at that time. Hidden until the first roll.
	for (int32 i = 0; i < 6; ++i)
	{
		USkeletalMeshComponent* L = CreateDefaultSubobject<USkeletalMeshComponent>(
			*FString::Printf(TEXT("LoadoutComp_%d"), i));
		L->SetupAttachment(BodyMesh);
		L->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		L->SetCastShadow(false);
		L->SetVisibility(false);
		if (i == 0)
		{
			L->SetUsingAbsoluteLocation(true);
			L->SetUsingAbsoluteRotation(true);
		}
		LoadoutComps.Add(L);
	}

	// Connector pool (wires/pipes linking each loadout part back to the body): up to 2 per
	// slot (12 total), each a UPoseableMeshComponent showing SK_WireChain -- a REAL 8-bone
	// skinned wire posed along the sag curve every frame in UpdateConnectors() (per the
	// user's explicit direction; see FKitConnectorState for the mechanism history). The mesh
	// is NOT assigned here -- BeginPlay assigns it behind a GetSkeleton() guard, same
	// cook-crash protection as RiggedBody above.
	{
		static ConstructorHelpers::FObjectFinder<USkeletalMesh> WireSK(
			TEXT("/LaserRifleMod/Equipment/LaserRifle/Rigged/SK_WireChain.SK_WireChain"));
		if (WireSK.Succeeded()) { WireChainMesh = WireSK.Object; }
		// Wire material: the sheathed-wire kit material (skeletal-usage flagged; BodyMaterial
		// is None on the CDO and the body mesh's material lacks the skeletal usage flag, so
		// either fallback would render default-gray on a skeletal wire in Shipping).
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> WireMat(
			TEXT("/LaserRifleMod/Equipment/LaserRifle/Components/Kit/M_Kit_wire_sheathed.M_Kit_wire_sheathed"));
		if (WireMat.Succeeded()) { WireChainMaterial = WireMat.Object; }
		for (int32 i = 0; i < 12; ++i)
		{
			UPoseableMeshComponent* C = CreateDefaultSubobject<UPoseableMeshComponent>(
				*FString::Printf(TEXT("LoadoutConnector_%d"), i));
			C->SetupAttachment(BodyMesh);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCastShadow(false);
			// ABSOLUTE placement (reverted from engine-attaching the wire under the part's
			// conn bone: the wire then inherits the part's scale chain + attach propagation
			// timing, and compensating per-frame compounds errors -- in-game: thick zigzag
			// wires flying off unanchored, same compounding-transform bug class as the
			// large-coil). Skeleton-to-skeleton is achieved by BONES DRIVING BONES instead:
			// wire_0 is pinned to the part's conn bone and the far end to the body's mount
			// bone every frame, via the same-frame GetSocketTransform readback the user
			// validated in the slot-0 dangle test.
			C->SetUsingAbsoluteLocation(true);
			C->SetUsingAbsoluteRotation(true);
			C->SetUsingAbsoluteScale(true);
			C->SetVisibility(false);
			LoadoutConnectors.Add(C);
			ConnectorStates.AddDefaulted();
		}
	}

	// Rigged poseable body (Mk1 pilot): driven world-absolute like BodyMesh, bones posed in
	// DriveRigBones. Hidden until a rigged Mk is shown (ApplyVisualsForLevel / the hold toggle).
	RiggedBody = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("RiggedBody"));
	RiggedBody->SetupAttachment(RootComponent);   // RootComponent == BodyMesh at this point
	RiggedBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RiggedBody->SetCastShadow(false);
	RiggedBody->SetUsingAbsoluteLocation(true);
	RiggedBody->SetUsingAbsoluteRotation(true);
	RiggedBody->SetUsingAbsoluteScale(true);
	RiggedBody->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> RigSK(
		TEXT("/LaserRifleMod/Equipment/LaserRifle/Rigged/SK_LaserRifle_Mk1c.SK_LaserRifle_Mk1c"));
	// NOTE: do NOT assign the mesh here. A skeletal mesh whose USkeleton failed to cook would
	// assert in UPoseableMeshComponent::AllocateTransformData the instant the component registers
	// (on equip/spawn) — a hard crash before BeginPlay. We assign it in SetupRig() behind a
	// valid-skeleton guard so a broken/uncooked asset degrades to the static BodyMesh instead.
	if (RigSK.Succeeded()) { RiggedMesh = RigSK.Object; }
	// The rigged Mk1 (SK_LaserRifle_Mk1c) ships with a plain default material -> it renders GRAY. Load the
	// Mk1 body material so SetupRig can apply it (the rig is the same Mk1 body + bones, so its UVs match
	// M_Rifle_Mk01). Fixes the untextured Mk1 while keeping the dangling-bone rig (user 2026-07-05).
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RigMatF(
		TEXT("/LaserRifleMod/Equipment/LaserRifle/Meshes_Tripo/M_Rifle_Mk01.M_Rifle_Mk01"));
	if (RigMatF.Succeeded()) { RiggedBodyMaterial = RigMatF.Object; }

	// Beam: thin emissive cylinder, world-absolute, hidden until a shot.
	BeamMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BeamMesh"));
	BeamMesh->SetupAttachment(RootComponent);
	BeamMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BeamMesh->SetCastShadow(false);
	BeamMesh->SetUsingAbsoluteLocation(true);
	BeamMesh->SetUsingAbsoluteRotation(true);
	BeamMesh->SetUsingAbsoluteScale(true);
	BeamMesh->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cyl.Succeeded()) { BeamMesh->SetStaticMesh(Cyl.Object); }

	// Unlit emissive beam material (authored by Scripts/ue/mkbeam.py).
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BeamMat(
		TEXT("/LaserRifleMod/Equipment/LaserRifle/M_LaserRifle_Beam.M_LaserRifle_Beam"));
	if (BeamMat.Succeeded()) { BeamMaterial = BeamMat.Object; }

	// Bake the Mk1..Mk10 beam colours so they never depend on an empty array.
	if (LevelBeamColors.Num() == 0)
	{
		LevelBeamColors = {
			FLinearColor(0.18f, 0.80f, 0.44f), // Mk1 emerald
			FLinearColor(0.30f, 1.00f, 0.30f), // Mk2 green
			FLinearColor(0.20f, 1.00f, 0.80f), // Mk3 spring
			FLinearColor(0.20f, 0.85f, 1.00f), // Mk4 cyan
			FLinearColor(0.25f, 0.45f, 1.00f), // Mk5 blue
			FLinearColor(0.45f, 0.30f, 1.00f), // Mk6 indigo
			FLinearColor(0.70f, 0.30f, 1.00f), // Mk7 violet
			FLinearColor(1.00f, 0.25f, 0.85f), // Mk8 magenta
			FLinearColor(1.00f, 0.35f, 0.15f), // Mk9 orange
			FLinearColor(1.00f, 0.95f, 0.80f)  // Mk10 white-hot
		};
	}

	// Heat smoke: pooled translucent spheres, world-positioned, hidden until used.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sph(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SmokeMat(
		TEXT("/LaserRifleMod/Equipment/LaserRifle/M_LaserRifle_Smoke.M_LaserRifle_Smoke"));
	if (SmokeMat.Succeeded()) { SmokeMaterial = SmokeMat.Object; }

	// Fire sound: custom laser "pew" authored in the Wwise project, cooked under the mod
	// (/LaserRifleMod/Audio/Play_LaserRifle_Fire -> Wwise event Play_LaserRifle_Fire,
	// media laser_zap_v1.wav baked into the LaserRifle SoundBank).
	static ConstructorHelpers::FObjectFinder<UAkAudioEvent> ZapSnd(
		TEXT("/LaserRifleMod/Audio/Play_LaserRifle_Fire.Play_LaserRifle_Fire"));
	if (ZapSnd.Succeeded()) { FireSound = ZapSnd.Object; }
	// One-time diagnostic: a single launch tells us if the custom event resolved.
	// <none> => path/cook problem (fall back to ShockShank); a name => asset resolved.
	LR_LOG(CVarLrLogGeneral, TEXT("[LR] FireSound=%s"),
		FireSound ? *FireSound->GetName() : TEXT("<none>"));
	for (int32 i = 0; i < 24; ++i)
	{
		UStaticMeshComponent* P = CreateDefaultSubobject<UStaticMeshComponent>(
			*FString::Printf(TEXT("Smoke_%d"), i));
		P->SetupAttachment(RootComponent);
		if (Sph.Succeeded()) { P->SetStaticMesh(Sph.Object); }
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		P->SetCastShadow(false);
		P->SetUsingAbsoluteLocation(true);
		P->SetUsingAbsoluteRotation(true);
		P->SetUsingAbsoluteScale(true);
		P->SetOnlyOwnerSee(true);
		P->SetVisibility(false);
		SmokeComps.Add(P);
		SmokePos.Add(FVector::ZeroVector); SmokeVel.Add(FVector::ZeroVector);
		SmokeAge.Add(1.f); SmokeLife.Add(1.f);   // age>=life => inactive
	}

	// Spark/flame pool (the low/mid-tier "shinies"); MIDs from the emissive beam material in BeginPlay.
	for (int32 i = 0; i < 32; ++i)
	{
		UStaticMeshComponent* S = CreateDefaultSubobject<UStaticMeshComponent>(
			*FString::Printf(TEXT("Spark_%d"), i));
		S->SetupAttachment(RootComponent);
		// CYLINDER (not the Plane) so the (Wid,Wid,Len) streak sizing actually elongates along the
		// spark's velocity -> a thin bright electric STREAK, not a flat quad (a Plane has no Z extent
		// so the length scale did nothing = tiny facing squares).
		if (Cyl.Succeeded()) { S->SetStaticMesh(Cyl.Object); }
		S->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		S->SetCastShadow(false);
		S->SetUsingAbsoluteLocation(true); S->SetUsingAbsoluteRotation(true); S->SetUsingAbsoluteScale(true);
		S->SetOnlyOwnerSee(true); S->SetVisibility(false);
			SparkComps.Add(S);
		SparkPos.Add(FVector::ZeroVector); SparkVel.Add(FVector::ZeroVector);
		SparkAge.Add(1.f); SparkLife.Add(1.f); SparkGrav.Add(220.f); SparkJitter.Add(0.f); SparkColor.Add(FLinearColor::White);
	}
	// Plasma orb (high-tier "shiny"): an emissive sphere that hovers near the emitter.
	PlasmaOrb = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlasmaOrb"));
	PlasmaOrb->SetupAttachment(RootComponent);
	if (Sph.Succeeded()) { PlasmaOrb->SetStaticMesh(Sph.Object); }
	PlasmaOrb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlasmaOrb->SetCastShadow(false);
	PlasmaOrb->SetUsingAbsoluteLocation(true); PlasmaOrb->SetUsingAbsoluteRotation(true); PlasmaOrb->SetUsingAbsoluteScale(true);
	PlasmaOrb->SetOnlyOwnerSee(true); PlasmaOrb->SetVisibility(false);

	// Electrical arc segment pool (thin cylinders; emissive beam-material MIDs in BeginPlay).
	for (int32 i = 0; i < NUM_ARCS * MAX_SEGS_PER_ARC; ++i)
	{
		UStaticMeshComponent* A = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Arc_%d"), i));
		A->SetupAttachment(RootComponent);
		if (Cyl.Succeeded()) { A->SetStaticMesh(Cyl.Object); }
		A->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		A->SetCastShadow(false);
		A->SetUsingAbsoluteLocation(true); A->SetUsingAbsoluteRotation(true); A->SetUsingAbsoluteScale(true);
		A->SetOnlyOwnerSee(true); A->SetVisibility(false);
		ArcSegs.Add(A);
	}
	for (int32 i = 0; i < NUM_ARCS; ++i)
	{
		ArcA.Add(FVector::ZeroVector); ArcB.Add(FVector::ZeroVector); ArcVel.Add(FVector::ZeroVector);
		ArcAge.Add(1.f); ArcLife.Add(1.f); ArcChaos.Add(1.f); ArcGens.Add(3); ArcColor.Add(FLinearColor::White);
	}

	// Held as an arms-slot weapon. The hold POSE and attach MODE are decided at
	// Equip time from config (default = floating viewmodel, no forced pose).
	mEquipmentSlot = EEquipmentSlot::ES_ARMS;
	mArmAnimation  = EArmEquipment::AE_None;

	// Native ammo HUD: declare our fuel-cell ammo class so the game's ammo widget reads
	// GetCurrentAmmo()/GetMagSize() off us. Firing still runs through our custom path; we
	// only keep mCurrentAmmoCount in sync. (The real Equip/magazine logic is in the closed
	// game binary, so the actual HUD visibility is verified in-game, not statically.)
	mAllowedAmmoClasses.Empty();
	mAllowedAmmoClasses.Add(ULaserRifleAmmo::StaticClass());
	mDesiredAmmoClass       = ULaserRifleAmmo::StaticClass();
	mCurrentAmmunitionClass = ULaserRifleAmmo::StaticClass();
	mCurrentAmmoCount       = 30;   // matches ShotsPerCell until first fire syncs it
}

void ALaserRifleWeapon::BeginPlay()
{
	Super::BeginPlay();
	SetupRig();         // build the rig bone cache before the first visual apply
	RefreshVisuals();

	// DIAGNOSTIC: confirm the beam cylinder survived the Shipping cook. If this
	// logs <none>, /Engine/BasicShapes was not packaged -> swap to a cooked mesh.
	LR_LOG(CVarLrLogBeam, TEXT("[LR] BeginPlay beamMesh=%s bodyMat=%s beamMat=%s colors=%d"),
		(BeamMesh && BeamMesh->GetStaticMesh()) ? *BeamMesh->GetStaticMesh()->GetName() : TEXT("<none>"),
		BodyMaterial ? *BodyMaterial->GetName() : TEXT("<none>"),
		BeamMaterial ? *BeamMaterial->GetName() : TEXT("<none>"),
		LevelBeamColors.Num());

	for (int32 i = 0; i < SmokeComps.Num(); ++i)
	{
		if (SmokeComps[i] && SmokeMaterial)
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(SmokeMaterial, this);
			SmokeComps[i]->SetMaterial(0, M);
			SmokeMIDs.Add(M);
		}
		else { SmokeMIDs.Add(nullptr); }
	}

	// Connector wires: assign SK_WireChain to the poseable pool (behind the same GetSkeleton
	// guard as RiggedBody -- a skeleton that failed to cook asserts on register), build the
	// bone cache, and give them the rifle's body material so they read as dark metal instead
	// of the import-default gray.
	bWireMeshReady = false;
	if (WireChainMesh && WireChainMesh->GetSkeleton())
	{
		// Ref component-space rotation/scale per bone: the chain is authored along +X, so the
		// per-frame pose only needs to swing each bone's ref rotation onto the live curve
		// direction (FindBetweenNormals(X, dir) * refRot) -- same composition style as
		// DriveRigBones.
		WireBoneNames.Reset(); WireRefRot.Reset(); WireRefScale.Reset();
		WireRefPos.Reset(); WireRefDir.Reset();
		const FReferenceSkeleton& Ref = WireChainMesh->GetRefSkeleton();
		const TArray<FTransform>& Pose = Ref.GetRefBonePose();
		auto CompSpace = [&Ref, &Pose](int32 BoneIdx)
		{
			FTransform T = Pose[BoneIdx];
			for (int32 P = Ref.GetParentIndex(BoneIdx); P != INDEX_NONE; P = Ref.GetParentIndex(P))
			{
				T = T * Pose[P];
			}
			return T;
		};
		bool bAllFound = true;
		for (int32 b = 0; b < 8; ++b)
		{
			const FName BoneName(*FString::Printf(TEXT("wire_%d"), b));
			const int32 Idx = Ref.FindBoneIndex(BoneName);
			if (Idx == INDEX_NONE) { bAllFound = false; break; }
			const FTransform T = CompSpace(Idx);
			WireBoneNames.Add(BoneName);
			WireRefRot.Add(T.GetRotation());
			WireRefScale.Add(T.GetScale3D());
			WireRefPos.Add(T.GetLocation());
		}
		// MEASURE the ref chain direction bone-to-bone (do not assume the FBX conversion kept
		// the Blender authoring axis). Last bone reuses the previous direction. Also cache
		// which BONE-LOCAL axis the chain runs along (for the per-bone stretch scale) and
		// the ref bone spacing.
		if (bAllFound)
		{
			WireLocalAxis.Reset();
			for (int32 b = 0; b < WireRefPos.Num(); ++b)
			{
				const int32 nb = FMath::Min(b + 1, WireRefPos.Num() - 1);
				const int32 pb = (nb == b) ? b - 1 : b;
				FVector D = (WireRefPos[nb] - WireRefPos[pb]).GetSafeNormal();
				if (D.IsNearlyZero()) { D = FVector::XAxisVector; }
				WireRefDir.Add(D);
				const FVector L = WireRefRot[b].Inverse().RotateVector(D).GetAbs();
				WireLocalAxis.Add((L.X >= L.Y && L.X >= L.Z) ? 0 : (L.Y >= L.Z ? 1 : 2));
			}
			WireRefSegLen = FMath::Max(FVector::Dist(WireRefPos[0], WireRefPos[1]), KINDA_SMALL_NUMBER);
			LR_LOG(CVarLrLogRig,
				TEXT("[LR] WireChain refDir[0]=(%.2f,%.2f,%.2f) refSpan=%.1fcm segLen=%.1fcm localAxis=%d (measured)"),
				WireRefDir[0].X, WireRefDir[0].Y, WireRefDir[0].Z,
				FVector::Dist(WireRefPos[0], WireRefPos.Last()), WireRefSegLen, WireLocalAxis[0]);
		}
		if (bAllFound)
		{
			UMaterialInterface* ConnSrc = WireChainMaterial ? WireChainMaterial.Get() : BodyMaterial.Get();
			for (UPoseableMeshComponent* C : LoadoutConnectors)
			{
				if (!C) { continue; }
				C->SetSkinnedAssetAndUpdate(WireChainMesh);
				if (ConnSrc) { C->SetMaterial(0, ConnSrc); }
			}
			bWireMeshReady = true;
		}
		LR_LOG(CVarLrLogRig, TEXT("[LR] WireChain: mesh=%s bones=%d ready=%d"),
			*GetNameSafe(WireChainMesh), WireBoneNames.Num(), bWireMeshReady ? 1 : 0);
	}
	else
	{
		UE_LOG(LogLaserRifle, Warning,
			TEXT("[LR] WireChain DISABLED: %s (no mesh or no skeleton -- cook issue?) -> no connector wires"),
			*GetNameSafe(WireChainMesh));
	}

	// Spark + plasma-orb MIDs from the unlit emissive beam material (BeamColor / Intensity params).
	UMaterialInterface* FxSrc = BeamMaterial ? BeamMaterial.Get() : BodyMaterial.Get();
	for (int32 i = 0; i < SparkComps.Num(); ++i)
	{
		if (SparkComps[i] && FxSrc)
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(FxSrc, this);
			SparkComps[i]->SetMaterial(0, M);
			SparkMIDs.Add(M);
		}
		else { SparkMIDs.Add(nullptr); }
	}
	for (int32 i = 0; i < ArcSegs.Num(); ++i)
	{
		if (ArcSegs[i] && FxSrc)
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(FxSrc, this);
			ArcSegs[i]->SetMaterial(0, M);
			ArcMIDs.Add(M);
		}
		else { ArcMIDs.Add(nullptr); }
	}
	if (PlasmaOrb && FxSrc)
	{
		PlasmaOrbMID = UMaterialInstanceDynamic::Create(FxSrc, this);
		PlasmaOrb->SetMaterial(0, PlasmaOrbMID);
	}
}

void ALaserRifleWeapon::Equip(AFGCharacterPlayer* character)
{
	Super::Equip(character);
	RefreshVisuals();
	EnsureNativeAmmo();   // (re)assert the fuel-cell ammo class so the native ammo HUD reads us
	// Always held-in-hands (the SML toggle didn't persist; held is the only good mode now).
	bUseArmsAttach = true;
	mArmAnimation = EArmEquipment::AE_Rifle;
	AttachToArms();
	LR_LOG(CVarLrLogRig, TEXT("[LR] Weapon Equip armsAttach=%d armAnim=%d bodyMesh=%s"),
		bUseArmsAttach ? 1 : 0, (int32)GetArmsAnimation(),
		(BodyMesh && BodyMesh->GetStaticMesh()) ? *BodyMesh->GetStaticMesh()->GetName() : TEXT("<none>"));
}

void ALaserRifleWeapon::UnEquip()
{
	GetWorldTimerManager().ClearTimer(BeamHandle);
	if (BeamMesh) { BeamMesh->SetVisibility(false); }
	if (Crosshair) { Crosshair->RemoveFromParent(); Crosshair = nullptr; bCrosshairAdded = false; }
	bAttached = false;
	Super::UnEquip();
}

void ALaserRifleWeapon::AttachToCamera()
{
	bAttached = false;
	if (!BodyMesh) { return; }
	AFGCharacterPlayer* Char = GetInstigatorCharacter();
	UCameraComponent* Cam = Char ? Char->FindComponentByClass<UCameraComponent>() : nullptr;
	if (!Cam)
	{
		UE_LOG(LogLaserRifle, Warning, TEXT("[LR] AttachToCamera: no camera component."));
		return;
	}
	BodyMesh->AttachToComponent(Cam, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	// Back to camera-relative transform (arms mode may have set these absolute).
	BodyMesh->SetUsingAbsoluteLocation(false);
	BodyMesh->SetUsingAbsoluteRotation(false);
	BodyMesh->SetUsingAbsoluteScale(false);
	BodyMesh->SetOnlyOwnerSee(true);
	BodyMesh->SetVisibility(true);
	bAttached = true;
	ApplyGripFromConfig();
	LR_LOG(CVarLrLogRig, TEXT("[LR] Body attached to CAMERA viewmodel -- live grip sliders active."));
}

void ALaserRifleWeapon::AttachToArms()
{
	if (!BodyMesh) { return; }
	AFGCharacterPlayer* Char = GetInstigatorCharacter();
	USkeletalMeshComponent* Arms = Char ? Char->GetMesh1P() : nullptr;
	if (!Arms)
	{
		UE_LOG(LogLaserRifle, Warning, TEXT("[LR] AttachToArms: no Mesh1P; falling back to camera."));
		AttachToCamera();
		return;
	}
	// DIAGNOSTIC: dump available sockets so we can pick the correct grip socket.
	FString Names;
	for (const FName& N : Arms->GetAllSocketNames()) { Names += N.ToString() + TEXT(", "); }
	const bool bHasSocket = Arms->DoesSocketExist(GripSocketName);
	LR_LOG(CVarLrLogRig, TEXT("[LR] Mesh1P sockets: %s"), *Names);

	BodyMesh->AttachToComponent(Arms, FAttachmentTransformRules::SnapToTargetNotIncludingScale, GripSocketName);
	BodyMesh->SetOnlyOwnerSee(true);
	BodyMesh->SetVisibility(true);
	if (RiggedBody) { RiggedBody->SetOnlyOwnerSee(true); }   // MP: owner-only viewmodel, like BodyMesh
	RigVelInit = false;                                      // restart rig motion tracking on (re)equip (no stale-accel whip)
	bAttached = true;
	bHeldInit = false;           // re-snap the smoothed hold to the new target
	ProceduralArmsHold(0.f);     // place it immediately (procedural: hand-follow + aim-forward)
	LR_LOG(CVarLrLogRig, TEXT("[LR] Body attached to arms socket '%s' (exists=%d) -- PROCEDURAL hold (aim-forward)"),
		*GripSocketName.ToString(), bHasSocket ? 1 : 0);
	// DIAGNOSTIC: where did everything end up? (camera vs socket vs rifle body) so we
	// can see WHY arms-mode hides the rifle (socket off-screen, or grip offset wrong).
	{
		const FVector Cam  = Char->GetCameraComponentWorldLocation();
		const FTransform SockX = Arms->GetSocketTransform(GripSocketName, RTS_World);
		const FVector Sock = SockX.GetLocation();
		const FVector Body = BodyMesh->GetComponentLocation();
		// Orientations (world basis) so we can COMPUTE the rotation to point the muzzle forward.
		const FVector CamFwd  = Char->GetActorForwardVector();
		const FVector SockFwd = SockX.GetUnitAxis(EAxis::X);
		const FVector SockUp  = SockX.GetUnitAxis(EAxis::Z);
		const FVector RifleFwd = BodyMesh->GetForwardVector();  // muzzle = +X local
		const FVector RifleUp  = BodyMesh->GetUpVector();
		LR_LOG(CVarLrLogRig,
			TEXT("[LR] ARMS-POS cam=%s socket=%s body=%s | body-cam=%.0f socket-cam=%.0f | rel=%s scale=%s vis=%d"),
			*Cam.ToCompactString(), *Sock.ToCompactString(), *Body.ToCompactString(),
			(Body - Cam).Size(), (Sock - Cam).Size(),
			*BodyMesh->GetRelativeLocation().ToCompactString(), *BodyMesh->GetRelativeScale3D().ToCompactString(),
			BodyMesh->IsVisible() ? 1 : 0);
		LR_LOG(CVarLrLogRig,
			TEXT("[LR] ARMS-DIR camFwd=%s sockFwd=%s sockUp=%s rifleFwd=%s rifleUp=%s | relRot=%s"),
			*CamFwd.ToCompactString(), *SockFwd.ToCompactString(), *SockUp.ToCompactString(),
			*RifleFwd.ToCompactString(), *RifleUp.ToCompactString(),
			*BodyMesh->GetRelativeRotation().ToCompactString());
	}
}

float ALaserRifleWeapon::GripCfg(const TCHAR* StrId, float DefaultValue) const
{
	const UWorld* W = GetWorld();
	const USessionSettingsManager* M = W ? W->GetSubsystem<USessionSettingsManager>() : nullptr;
	return M ? M->GetFloatOptionValue(StrId) : DefaultValue;
}

bool ALaserRifleWeapon::ConfigBool(const TCHAR* StrId, bool DefaultValue) const
{
	const UWorld* W = GetWorld();
	const USessionSettingsManager* M = W ? W->GetSubsystem<USessionSettingsManager>() : nullptr;
	return M ? M->GetBoolOptionValue(StrId) : DefaultValue;
}

void ALaserRifleWeapon::ApplyGripFromConfig()
{
	if (!BodyMesh || !bAttached) { return; }
	using namespace LaserRifleSettings;
	// Baked default placement (dialed-in values). The manual grip-override sliders were removed
	// 2026-07-05 (superseded by baked per-Mk placement + console dials), so this is the placement.
	const float Sc = 1.0f;
	const FRotator Rot = FRotator::ZeroRotator;
	const FVector  Loc(51.f, 10.f, -20.f);
	BodyMesh->SetRelativeLocation(Loc);
	BodyMesh->SetRelativeRotation(Rot);
	BodyMesh->SetRelativeScale3D(FVector(Sc));
}

void ALaserRifleWeapon::ProceduralArmsHold(float Dt)
{
	if (!BodyMesh) { return; }
	AFGCharacterPlayer* C = GetInstigatorCharacter();
	USkeletalMeshComponent* Arms = C ? C->GetMesh1P() : nullptr;
	if (!C || !Arms) { return; }
	// Advance the cell-swap motion clock. Dt==0 on immediate placement calls (AttachToArms)
	// so it won't advance there — correct.
	if (bSwapping)
	{
		RechargeElapsed += Dt;
		if (RechargeElapsed >= SwapDownTime + SwapHoldTime + SwapUpTime) { bSwapping = false; }
	}
	using namespace LaserRifleSettings;
	// Defaults are in VIEW space: +X forward (along aim), +Y right, +Z up. So the
	// sliders are intuitive now (Forward/Back really moves it forward, etc.).
	// Defaults = the player's dialed-in hold (forward 20, centered, up 15, scale 0.6).
	float Sc = 0.6f;
	// Per-tier hold-scale override (<=0 = keep 0.6). Lets a chunkier/leaner mesh be sized
	// right in-hand per Mk without re-export; the global GripOverride sliders still win if on.
	{
		const int32 VL = EffectiveMkLevel();
		const int32 SIdx = FMath::Clamp(VL, 1, 10) - 1;
		if (LevelHoldScales.IsValidIndex(SIdx) && LevelHoldScales[SIdx] > 0.01f) { Sc = LevelHoldScales[SIdx]; }
	}
	FRotator Fine = FRotator::ZeroRotator;
	FVector Off(20.f, 0.f, 15.f);   // forward, right, up (view space)
	// Per-Mk baked held-orientation correction + live tuning (lr.HoldPitch/Yaw/Roll). Tilts a body
	// whose sculpted barrel doesn't sit along mesh +X so the HELD gun points at the crosshair (user:
	// "mk8 looks pointed lower than the crosshair; mk9 points at it"). Applied in mesh-local space.
	{
		const int32 FIdx = FMath::Clamp(EffectiveMkLevel(), 1, 10) - 1;
		if (LevelFineRot.IsValidIndex(FIdx)) { Fine = LevelFineRot[FIdx]; }
		Fine += FRotator(CVarHoldPitch.GetValueOnGameThread(),
			CVarHoldYaw.GetValueOnGameThread(), CVarHoldRoll.GetValueOnGameThread());
	}
	// (Manual grip-override settings removed 2026-07-05: placement is baked per-Mk [Sc from
	//  LevelHoldScales, Fine from LevelFineRot + lr.HoldPitch/Yaw/Roll CVars, Off default] -- the
	//  in-menu grip sliders were superseded dev-tuning and are gone.)
	// --- Cell-swap motion: dip down -> HOLD at the bottom (cell swaps) -> return, layered
	//     additively on the hold. Idle (!bSwapping) => every term zero => held pose byte-
	//     identical. Down/up ease keep the in/out speed; the hold is the deliberate swap beat. ---
	FRotator SwapRot = FRotator::ZeroRotator;
	if (bSwapping)
	{
		const float down = FMath::Max(0.01f, SwapDownTime);
		const float up   = FMath::Max(0.01f, SwapUpTime);
		const float t    = RechargeElapsed;
		float present;   // 0 -> 1 ease in, HOLD at 1, 1 -> 0 ease out
		if (t < down)                     { const float u = t / down; present = u * u * (3.f - 2.f * u); }   // ease in 0->1
		else if (t < down + SwapHoldTime) { present = 1.f; }                                                 // HOLD presented
		else { const float u = FMath::Clamp((t - down - SwapHoldTime) / up, 0.f, 1.f);
			present = 1.f - u * u * (3.f - 2.f * u); }                                                       // ease out 1->0
		// "Working the cell" wobble while held down, so the presented pose isn't frozen.
		const bool  bHold  = (t >= down && t < down + SwapHoldTime);
		const float wobble = bHold ? FMath::Sin((t - down) * 9.f) : 0.f;
		// Keep the rifle OUT FRONT and visible (lift + slight forward); muzzle tilts UP and
		// swings to the LEFT, with a working wobble — like the off-hand is seating a cell.
		Off.X += SwapPresentFwd * present;
		Off.Z += SwapPresentUp  * present;
		SwapRot = FRotator(
			SwapTiltPitch * present + 2.0f * wobble,   // pitch: muzzle up (+ wobble)
			-SwapYawLeft  * present,                   // yaw: muzzle across to the LEFT
			SwapTiltRoll  * present + 3.0f * wobble);  // roll: hand turn (+ wobble)
		if (t >= down && !bSwapMidLogged) { bSwapMidLogged = true;
			LR_LOG(CVarLrLogRig, TEXT("[LR] CellSwap presented: t=%.2f hold=%.2fs pitch=%.1f yawL=%.1f"),
				t, SwapHoldTime, SwapTiltPitch, SwapYawLeft); }
	}
	else { bSwapMidLogged = false; }   // re-arm for the next swap
	// Orientation: muzzle (+X local) points where the player looks -> always forward,
	// independent of the hand bone's wildly-animating rotation.
	const FRotator Aim = FRotationMatrix::MakeFromX(C->GetCameraComponentForwardVector()).Rotator();
	// Position: anchored to the hand socket (so it rides the arm anim -> jump/equip
	// cohesion), offset in view space, lightly smoothed for weight.
	const FVector Hand   = Arms->GetSocketLocation(GripSocketName);
	// Instant follow (no smoothing): position-lag behind the instant aim-rotation was
	// the turn judder. The hand socket is already animated for the frame (we tick in
	// TG_PostUpdateWork), so tracking it directly is as smooth as a bone attach.
	HeldLoc = Hand + Aim.RotateVector(Off);
	bHeldInit = true;
	// Drive world-absolute so the socket's unstable rotation can't fight us.
	BodyMesh->SetUsingAbsoluteLocation(true);
	BodyMesh->SetUsingAbsoluteRotation(true);
	BodyMesh->SetUsingAbsoluteScale(true);
	const FQuat WorldQ = Aim.Quaternion() * Fine.Quaternion() * SwapRot.Quaternion();   // SwapRot = identity when idle
	BodyMesh->SetWorldLocationAndRotation(HeldLoc, WorldQ);
	BodyMesh->SetWorldScale3D(FVector(Sc));

	// --- Rigged body: live-toggle (lr.RigEnable) static<->rigged, mirror the held transform,
	//     then pose the antenna/battery/wire bones procedurally on top. ---
	{
		// Enforce body visibility EVERY frame (not just on change): AttachToArms force-sets
		// BodyMesh visible on every (re)attach, so a change-only toggle would leave BOTH bodies
		// shown — the static textured mesh then draws over the rig (looks like "nothing changed").
		// Idempotent SetVisibility early-outs internally, so per-frame is cheap.
		const bool bWantRig = (CVarRigEnable.GetValueOnGameThread() != 0) && RiggedMesh && RiggedBody
			&& (EffectiveMkLevel() == 1);
		bRiggedActive = bWantRig;
		if (RiggedBody) { RiggedBody->SetVisibility(bWantRig); }
		BodyMesh->SetVisibility(!bWantRig);
		// Loadout components only make sense after a roll has happened -- same idempotent
		// every-frame enforcement as BodyMesh/RiggedBody. Slot 0 = proven GetSocketTransform
		// readback test; slot 1 = the ACTUAL unproven AttachToComponent-by-bone test. Both
		// visible ONLY while the rig is active, side by side for direct comparison; slots 2-5
		// stay static-path-only, unchanged.
		for (int32 li = 0; li < LoadoutComps.Num(); ++li)
		{
			USkeletalMeshComponent* L = LoadoutComps[li];
			if (!L) { continue; }
			const bool bWant = bHasRolledLoadout && ((li == 0 || li == 1) ? bWantRig : !bWantRig);
			L->SetVisibility(bWant);
		}
		if (bWantRig && RiggedBody)
		{
			// Normalize the rig to the 110cm rifle convention, then apply the same hold scale Sc the
			// static mesh uses. (RigNativeLongest auto-corrects whatever scale the import baked in.)
			const float RigScale = Sc * (110.f / FMath::Max(RigNativeLongest, KINDA_SMALL_NUMBER));
			// Orientation correction (the skeletal export/import flips axes vs the static-mesh path).
			// Applied in the rig's LOCAL space so its barrel points along aim. Live-tunable CVars.
			const FQuat RigFix = FRotator(CVarRigPitch.GetValueOnGameThread(),
				CVarRigYaw.GetValueOnGameThread(), CVarRigRoll.GetValueOnGameThread()).Quaternion();
			const FQuat RigWorldQ = WorldQ * RigFix;
			RiggedBody->SetWorldLocationAndRotation(HeldLoc, RigWorldQ);
			RiggedBody->SetWorldScale3D(FVector(RigScale));
			DriveRigBones(Dt, FTransform(RigWorldQ, HeldLoc, FVector(RigScale)));
		}
	}
}

void ALaserRifleWeapon::SetupRig()
{
	bRigSetup = false;
	RigBoneNames.Reset(); RigRefLocal.Reset(); RigAngle.Reset(); RigAngVel.Reset();
	if (!RiggedMesh || !RiggedBody) { return; }
	// Guard: a skeletal mesh with no USkeleton asserts in AllocateTransformData on register.
	// Assign the mesh now (deferred from the ctor) only if its skeleton is valid; otherwise null
	// RiggedMesh so the rig toggle can never activate and we stay on the static BodyMesh.
	if (!RiggedMesh->GetSkeleton())
	{
		UE_LOG(LogLaserRifle, Warning,
			TEXT("[LR] Rig DISABLED: %s has no skeleton (asset/cook issue) -> static body fallback"),
			*GetNameSafe(RiggedMesh));
		RiggedMesh = nullptr;
		return;
	}
	RiggedBody->SetSkinnedAssetAndUpdate(RiggedMesh);
	// Texture the rigged Mk1 with the Mk1 body material so it's not gray (user 2026-07-05). Same body+UVs
	// as the static Rifle_Mk01, so M_Rifle_Mk01 maps correctly; verify in-game (if UVs are off it just
	// looks wrong, not a crash).
	if (RiggedBodyMaterial)
	{
		const int32 NumMat = RiggedBody->GetNumMaterials();
		for (int32 m = 0; m < FMath::Max(1, NumMat); ++m) { RiggedBody->SetMaterial(m, RiggedBodyMaterial); }
		LR_LOG(CVarLrLogRig, TEXT("[LR] Rigged Mk1 material applied: %s (%d slots)"),
			*GetNameSafe(RiggedBodyMaterial), NumMat);
	}
	// Measure the rig mesh's actual size so we can normalize it to the 110cm rifle convention at
	// runtime — the headless skeletal import baked an unknown scale (it came in ~1cm = invisible).
	{
		const FBoxSphereBounds B = RiggedMesh->GetBounds();
		const float Longest = 2.f * FMath::Max3(B.BoxExtent.X, B.BoxExtent.Y, B.BoxExtent.Z);
		RigNativeLongest = (Longest > KINDA_SMALL_NUMBER) ? Longest : 110.f;
	}
	const FReferenceSkeleton& Ref = RiggedMesh->GetRefSkeleton();
	const TArray<FTransform>& Pose = Ref.GetRefBonePose();   // LOCAL bone transforms
	// Component-space transform of a bone = local * parentLocal * ... (UE: comp = local * parentComp).
	auto CompXf = [&](int32 bi) -> FTransform
	{
		FTransform t = FTransform::Identity;
		for (int32 cur = bi; cur != INDEX_NONE; cur = Ref.GetParentIndex(cur))
		{
			if (!Pose.IsValidIndex(cur)) { break; }
			t = t * Pose[cur];   // child first: comp = local_child * local_parent * ...
		}
		return t;
	};
	const int32 RootIdx = Ref.FindBoneIndex(TEXT("root"));
	RigRefCompRoot = (RootIdx != INDEX_NONE) ? CompXf(RootIdx) : FTransform::Identity;

	RigBoneNames = { TEXT("antenna_0"), TEXT("antenna_1"), TEXT("pend_0"), TEXT("pend_1"), TEXT("pend_2") };
	int32 found = 0;
	for (const FName& BN : RigBoneNames)
	{
		const int32 BI = Ref.FindBoneIndex(BN);
		RigRefLocal.Add((BI != INDEX_NONE && Pose.IsValidIndex(BI)) ? Pose[BI] : FTransform::Identity);
		RigAngle.Add(FVector2D::ZeroVector);
		RigAngVel.Add(FVector2D::ZeroVector);
		if (BI != INDEX_NONE) { ++found; }
	}
	// Per-instance randomized feel: seed off the actor id so each spawned rifle jiggles differently.
	FRandomStream RS((int32)(GetUniqueID() * 2654435761u + 12345u));
	RigIdleFreq  = RS.FRandRange(1.0f, 1.7f);
	RigIdlePhase = RS.FRandRange(0.f, 6.2831853f);
	RigIdleAmp   = RS.FRandRange(0.03f, 0.06f);
	RigStiff     = RS.FRandRange(75.f, 110.f);
	RigDamp      = RS.FRandRange(7.f, 11.f);
	RigLag       = RS.FRandRange(0.45f, 0.65f);
	RigVelInit = false; RigTime = 0.f;
	bRigSetup = (found > 0);
	// Body mount bones (mount_1..mount_6, added 2026-07-03): wire body-ends anchor to these
	// on the rig path. All 6 or none -- a partial set means a bad export, fall back entirely.
	{
		int32 MountsFound = 0;
		for (int32 mi = 1; mi <= 6; ++mi)
		{
			if (Ref.FindBoneIndex(FName(*FString::Printf(TEXT("mount_%d"), mi))) != INDEX_NONE) { ++MountsFound; }
		}
		bRigHasMountBones = (MountsFound == 6);
		LR_LOG(CVarLrLogRig, TEXT("[LR] Rig mount bones: %d/6 found -> bodyBoneAnchors=%d"),
			MountsFound, bRigHasMountBones ? 1 : 0);
	}
	LR_LOG(CVarLrLogRig,
		TEXT("[LR] Rig setup: bonesFound=%d/5 nativeLongest=%.2fcm stiff=%.1f damp=%.1f lag=%.2f idle(f=%.2f a=%.3f) root=%d"),
		found, RigNativeLongest, RigStiff, RigDamp, RigLag, RigIdleFreq, RigIdleAmp, RootIdx);
}

TArray<FKitConnectorSpec> ALaserRifleWeapon::InferConnectorSpecs(const USkeletalMesh* Part) const
{
	TArray<FKitConnectorSpec> Specs;
	if (!Part) { return Specs; }

	// TYPE by asset name (unchanged); ANCHORS from the part's own authored conn_N bones.
	const FString Name = Part->GetName().ToLower();
	EKitConnectorType Type = EKitConnectorType::StructuralWire;   // default: most parts get a wire.

	if (Name.Contains(TEXT("grip")) || Name.Contains(TEXT("strap")) || Name.Contains(TEXT("hatch"))
		|| Name.Contains(TEXT("spool")) || Name.Contains(TEXT("shroud")) || Name.Contains(TEXT("counterweight")))
	{
		Type = EKitConnectorType::None;   // fits flush/mounted directly on the body -- no connector.
	}
	else if (Name.Contains(TEXT("coolant")) || Name.Contains(TEXT("fluid")) || Name.Contains(TEXT("hose")))
	{
		Type = EKitConnectorType::FluidPipe;
	}
	else if (Name.Contains(TEXT("battery")) || Name.Contains(TEXT("core")) || Name.Contains(TEXT("capacitor"))
		|| Name.Contains(TEXT("cell")))
	{
		Type = EKitConnectorType::PowerWire;
	}
	// else (antenna, vent, wire bundle, readout, etc.): default StructuralWire (mounting/signal).

	if (Type == EKitConnectorType::None) { return Specs; }

	// Anchors: ref-pose component-space positions of the part skeleton's authored conn_0/
	// conn_1 bones (rigged in the Blender batch; elongated parts carry 2 conn bones at the
	// long-axis ends, compact parts 1 at the bbox center -- the 1-vs-2 decision was made at
	// authoring time). A part with no conn bones gets no connectors.
	const FReferenceSkeleton& Ref = Part->GetRefSkeleton();
	const TArray<FTransform>& Pose = Ref.GetRefBonePose();
	auto CompSpace = [&Ref, &Pose](int32 BoneIdx)
	{
		FTransform T = Pose[BoneIdx];
		for (int32 P = Ref.GetParentIndex(BoneIdx); P != INDEX_NONE; P = Ref.GetParentIndex(P))
		{
			T = T * Pose[P];
		}
		return T;
	};
	for (int32 ci = 0; ci < 2; ++ci)
	{
		const FName BoneName(*FString::Printf(TEXT("conn_%d"), ci));
		const int32 Idx = Ref.FindBoneIndex(BoneName);
		if (Idx == INDEX_NONE) { break; }
		FKitConnectorSpec S;
		S.Type = Type;
		S.LocalOffset = CompSpace(Idx).GetLocation();
		S.BoneName = BoneName;
		Specs.Add(S);
	}
	return Specs;
}

void ALaserRifleWeapon::RollRandomComponents()
{
	if (!BodyMesh || !BodyMesh->GetStaticMesh())
	{
		UE_LOG(LogLaserRifle, Warning, TEXT("[LR] RandomComponents: no BodyMesh/static mesh yet, skipped."));
		return;
	}
	if (KitComponentPool.Num() < 6)
	{
		UE_LOG(LogLaserRifle, Warning,
			TEXT("[LR] RandomComponents: KitComponentPool has %d entries (need >=6) -- import the ")
			TEXT("kit component pool and populate the CDO first. No-op."), KitComponentPool.Num());
		return;
	}

	// Pick 6 DISTINCT random indices (partial Fisher-Yates: shuffle a copy, take the first 6).
	TArray<int32> Idx;
	Idx.Reserve(KitComponentPool.Num());
	for (int32 i = 0; i < KitComponentPool.Num(); ++i) { Idx.Add(i); }
	for (int32 i = Idx.Num() - 1; i > 0; --i)
	{
		const int32 j = FMath::RandRange(0, i);
		Idx.Swap(i, j);
	}

	// Same 6 mount-fraction points as the Blender kit pipeline (component-kit-body-prompts.md):
	// {forward 0..1 along the full X range, lateral -1..1 about center Y, vertical -1..1 about center Z}.
	struct FMountFrac { float Fwd, Lat, Vert; const TCHAR* Name; };
	static const FMountFrac Mounts[6] = {
		{ 0.65f, -0.25f,  0.00f, TEXT("mount_1 (left foregrip)") },
		{ 0.45f,  0.00f,  0.30f, TEXT("mount_2 (top of receiver)") },
		{ 0.80f,  0.00f,  0.20f, TEXT("mount_3 (forward barrel top)") },
		{ 0.70f,  0.25f,  0.00f, TEXT("mount_4 (barrel side)") },
		{ 0.15f,  0.00f,  0.20f, TEXT("mount_5 (top of stock)") },
		{ 0.35f, -0.20f, -0.15f, TEXT("mount_6 (side of grip)") },
	};

	const FBox BodyLB = BodyMesh->GetStaticMesh()->GetBoundingBox();
	const FVector BCen = BodyLB.GetCenter(), BExt = BodyLB.GetExtent();
	const float BodyLongest = 2.f * BExt.GetMax();

	// Reset the connector pool: deactivate + hide + reparent back to BodyMesh, so a wire that
	// was bone-attached to a part from a prior roll doesn't stay riding it.
	for (int32 i = 0; i < ConnectorStates.Num(); ++i)
	{
		ConnectorStates[i] = FKitConnectorState();
		if (LoadoutConnectors.IsValidIndex(i) && LoadoutConnectors[i])
		{
			LoadoutConnectors[i]->AttachToComponent(BodyMesh, FAttachmentTransformRules::KeepWorldTransform);
			LoadoutConnectors[i]->SetVisibility(false);
		}
	}
	int32 ConnectorCursor = 0;

	FString RollLog;
	for (int32 m = 0; m < 6; ++m)
	{
		USkeletalMesh* Part = KitComponentPool[Idx[m]];
		USkeletalMeshComponent* Comp = LoadoutComps.IsValidIndex(m) ? LoadoutComps[m] : nullptr;
		if (!Part || !Comp) { continue; }
		// Same cook-crash guard as RiggedBody/WireChain: a skeletal mesh whose skeleton failed
		// to cook asserts in AllocateTransformData the moment it's assigned to a component.
		if (!Part->GetSkeleton())
		{
			UE_LOG(LogLaserRifle, Warning, TEXT("[LR] RandomComponents: %s has no skeleton (cook issue), slot %d skipped"),
				*Part->GetName(), m);
			continue;
		}

		const FMountFrac& Mnt = Mounts[m];
		const FVector LocalPos(
			BodyLB.Min.X + Mnt.Fwd * (BodyLB.Max.X - BodyLB.Min.X),
			BCen.Y + Mnt.Lat * BExt.Y,
			BCen.Z + Mnt.Vert * BExt.Z);

		// Self-correcting scale (same pattern as RigNativeLongest): target ~15% of the body's
		// own longest dimension, regardless of whatever raw scale the part imported at.
		const FVector PartExt = Part->GetBounds().BoxExtent;
		const float PartNativeLongest = 2.f * PartExt.GetMax();
		const float Scale = (PartNativeLongest > KINDA_SMALL_NUMBER)
			? (0.15f * BodyLongest) / PartNativeLongest : 1.f;

		Comp->SetSkeletalMeshAsset(Part);

		// Slot 1 = the ACTUAL unproven mechanism the cold review flagged (no precedent in this
		// codebase): true engine-level AttachToComponent by bone name on RiggedBody, letting UE
		// handle following the bone every tick with NO manual per-frame code (unlike slot 0's
		// proven GetSocketTransform readback in DriveRigBones). Explicit test of whether the
		// review's caution was warranted or overly conservative. Falls back to the normal rigid
		// static-mount behavior if the rig isn't set up yet (rolled before ever enabling it).
		if (m == 1 && RiggedBody && bRigSetup && RigBoneNames.IsValidIndex(4))
		{
			Comp->AttachToComponent(RiggedBody, FAttachmentTransformRules::SnapToTargetNotIncludingScale, RigBoneNames[4]);
			Comp->SetRelativeLocation(FVector::ZeroVector);
			Comp->SetRelativeRotation(FRotator::ZeroRotator);
			// WORLD scale, not relative: RiggedBody itself carries a large self-correcting scale
			// factor (RigScale, normalizing the skeletal mesh's tiny native import size up to
			// 110cm -- see SetupRig/RigNativeLongest). A RELATIVE scale here would compound with
			// that factor, producing a massively oversized result (confirmed in-game: "the large
			// coil" filling the screen). SetWorldScale3D specifies the desired ABSOLUTE size
			// directly, so it's correct regardless of whatever scale the bone's parent chain carries.
			Comp->SetWorldScale3D(FVector(Scale));
			RollLog += FString::Printf(TEXT("%s(BONE-ATTACH TEST)->%s "), Mnt.Name, *Part->GetName());
		}
		else if (m == 0)
		{
			// LoadoutComps[0] is bUsingAbsoluteLocation/Rotation=true (set in the constructor so
			// DriveRigBones can drive its world transform directly for the dangle-test slot).
			// SetRelativeLocation()/SetRelativeRotation() on an absolute component set the WORLD
			// transform verbatim, NOT relative to BodyMesh -- using the raw mesh-local LocalPos
			// here put the part (and its connector's anchor) near the MAP's world origin while the
			// actual held rifle sits wherever the player is (often kilometers away out in the
			// world). Confirmed via the [LR] Connector diagnostic log: len=378983cm. Harmless for
			// the part ITSELF (this slot is only ever visible when the rig is on, at which point
			// DriveRigBones overwrites it every frame with the correct bone-tracked position
			// before it's ever seen) -- but the new connector code sampled this transform
			// immediately, before that correction could run. Fix: convert to world space explicitly.
			const FTransform BodyXf = BodyMesh->GetComponentTransform();
			Comp->SetRelativeLocation(BodyXf.TransformPosition(LocalPos));
			Comp->SetRelativeRotation(BodyXf.GetRotation().Rotator());
			Comp->SetRelativeScale3D(FVector(Scale));
			RollLog += FString::Printf(TEXT("%s->%s "), Mnt.Name, *Part->GetName());
		}
		else
		{
			Comp->SetRelativeLocation(LocalPos);
			Comp->SetRelativeRotation(FRotator::ZeroRotator);
			Comp->SetRelativeScale3D(FVector(Scale));
			RollLog += FString::Printf(TEXT("%s->%s "), Mnt.Name, *Part->GetName());
		}

		// Connectors: just record which part+offsets each active connector should track.
		// UpdateConnectors() computes the actual live geometry every frame (see FKitConnectorState
		// for why this replaced an earlier UCableComponent-physics attempt).
		const TArray<FKitConnectorSpec> ConnSpecs = InferConnectorSpecs(Part);
		for (int32 ci = 0; ci < ConnSpecs.Num(); ++ci)
		{
			const FKitConnectorSpec& Spec = ConnSpecs[ci];
			if (!ConnectorStates.IsValidIndex(ConnectorCursor)) { break; }
			FKitConnectorState& State = ConnectorStates[ConnectorCursor];
			State.PartComp = Comp;
			State.PartLocalOffset = Spec.LocalOffset;
			State.PartBoneName = Spec.BoneName;
			++ConnectorCursor;
			// Two-connector (elongated) parts: spread the body-side anchors apart along the
			// body's length instead of both terminating at the same mount point. With a shared
			// endpoint, the wire from the part's NEAR end starts essentially at the body and
			// reads as a stub "connected only to the body" (user-observed); distinct anchors
			// make each wire span body->part like a real pair of leads.
			FVector BodyAnchor = LocalPos;
			if (ConnSpecs.Num() >= 2)
			{
				BodyAnchor.X += (ci == 0 ? -1.f : 1.f) * BExt.X * 0.16f;
				BodyAnchor.X = FMath::Clamp(BodyAnchor.X, BodyLB.Min.X, BodyLB.Max.X);
			}
			State.BodyLocalOffset = BodyAnchor;
			State.BodyBoneName = FName(*FString::Printf(TEXT("mount_%d"), m + 1));
			State.Type = Spec.Type;
			State.OwnerMountIndex = m;
			State.bActive = true;

			// Diagnostic: preview the world-space span this connector will draw, using the
			// SAME transforms/math UpdateConnectors uses every frame. If discs/spin ever show
			// up again, this line says exactly which connector + what Len it computed.
			const FVector DiagStart = Comp->GetComponentTransform().TransformPosition(Spec.LocalOffset);
			const FVector DiagEnd = BodyMesh->GetComponentTransform().TransformPosition(BodyAnchor);
			LR_LOG(CVarLrLogRig,
				TEXT("[LR] Connector[%d] mount=%d type=%d part=%s len=%.1fcm (hideBelow=6cm)"),
				ConnectorCursor - 1, m, (int32)Spec.Type, *Part->GetName(),
				FVector::Dist(DiagStart, DiagEnd));
		}
	}
	// Diagnostic: where do the rigged body's mount bones ACTUALLY sit, in the rig's own
	// component space (cm, fractions relative to its bounds)? If wires "aim at the wrong
	// spot while swinging", this line says whether the Blender axis mapping put the bones in
	// the right place -- data instead of another screenshot guess.
	if (bRigHasMountBones && RiggedBody && bRigSetup)
	{
		FString MountLog;
		for (int32 mi = 1; mi <= 6; ++mi)
		{
			const FName MB(*FString::Printf(TEXT("mount_%d"), mi));
			const FVector P = RiggedBody->GetSocketTransform(MB, RTS_Component).GetLocation();
			MountLog += FString::Printf(TEXT("m%d=(%.0f,%.0f,%.0f) "), mi, P.X, P.Y, P.Z);
		}
		LR_LOG(CVarLrLogRig, TEXT("[LR] Rig mount bones comp-space: %s(rigNativeLongest=%.0fcm)"),
			*MountLog, RigNativeLongest);
	}

	bHasRolledLoadout = true;
	// Visibility is enforced every frame by the same bWantRig blocks that toggle BodyMesh/
	// RiggedBody (ProceduralArmsHold + ApplyVisualsForLevel) -- no need to set it here too.
	LR_LOG(CVarLrLogRig, TEXT("[LR] RandomComponents rolled: %s (bodyLongest=%.1fcm, connectors=%d/%d)"),
		*RollLog, BodyLongest, ConnectorCursor, ConnectorStates.Num());
}

void ALaserRifleWeapon::UpdateConnectors(float Dt)
{
	if (!BodyMesh || !bWireMeshReady) { return; }
	const float SimDt = FMath::Clamp(Dt, 0.001f, 0.04f);   // same hitch clamp as DriveRigBones
	for (int32 i = 0; i < ConnectorStates.Num(); ++i)
	{
		FKitConnectorState& State = ConnectorStates[i];
		UPoseableMeshComponent* Wire = LoadoutConnectors.IsValidIndex(i) ? LoadoutConnectors[i] : nullptr;
		USkeletalMeshComponent* Part = State.PartComp.Get();
		if (!Wire) { continue; }
		const bool bWant = State.bActive && Part
			&& LoadoutComps.IsValidIndex(State.OwnerMountIndex) && LoadoutComps[State.OwnerMountIndex]
			&& LoadoutComps[State.OwnerMountIndex]->IsVisible();
		if (!bWant) { Wire->SetVisibility(false); continue; }

		// Part-side anchor = the part's conn_N BONE, read same-frame (bones driving bones --
		// the skeleton-to-skeleton link). Mount 1 on the rig composes from RiggedBody's
		// pend_2 bone directly (the part component's transform is a frame stale there).
		FVector Start;
		if (State.OwnerMountIndex == 1 && bRiggedActive && RiggedBody && bRigSetup
			&& RigBoneNames.IsValidIndex(4))
		{
			const FTransform BoneXf = RiggedBody->GetSocketTransform(RigBoneNames[4], RTS_World);
			Start = BoneXf.GetLocation()
				+ BoneXf.GetRotation().RotateVector(State.PartLocalOffset * Part->GetComponentScale().X);
		}
		else
		{
			Start = Part->GetSocketTransform(State.PartBoneName, RTS_World).GetLocation();
		}
		// Body-side anchor: on the rig path, the RIGGED body's mount_N BONE (skeleton-to-
		// skeleton at the body end too -- fixes the subtle wrong of anchoring to the HIDDEN
		// static body's frame, which differs from the rig's by RigFix). Static path keeps the
		// mesh-local mount point (the static body has no bones).
		const FVector End = (bRiggedActive && bRigHasMountBones && RiggedBody)
			? RiggedBody->GetSocketTransform(State.BodyBoneName, RTS_World).GetLocation()
			: BodyMesh->GetComponentTransform().TransformPosition(State.BodyLocalOffset);
		const FVector Delta = End - Start;
		const float Len = Delta.Size();
		// Below ~6cm there is no meaningful wire to draw: a compact part placed AT its mount
		// point leaves its anchor essentially ON the body anchor; the measured noise floor for
		// that case was ~3.5cm (transform-composition rounding -- [LR] Connector diag log).
		if (Len < 6.f) { Wire->SetVisibility(false); continue; }

		// Pose the bone chain along the sag curve in COMPONENT space, with the ABSOLUTE
		// component placed at Start with identity rotation and unit scale (component space ==
		// world offsets from Start -- the proven skelparts-3 geometry; no parent chain to
		// compound with). wire_0 sits on the part's conn bone, the last bone reaches the
		// body's mount bone. Curve: straight lerp plus a sine dip toward the gun's local
		// "down"; FluidPipe stays taut.
		Wire->SetWorldLocationAndRotation(Start, FQuat::Identity);
		Wire->SetWorldScale3D(FVector(1.f));
		const float SagAmount = (State.Type == EKitConnectorType::FluidPipe) ? 0.f
			: FMath::Clamp(Len * 0.4f, 1.f, 25.f);
		// WORLD down, not gun-local down: gun-local made the sag rotate with the rifle, so
		// pointing the muzzle up visibly flipped the drape upward ("reverse gravity",
		// user-observed). Real wires hang toward real gravity regardless of rifle pitch.
		const FVector SagDir = FVector(0.f, 0.f, -1.f);

		// Live sway: spring-damper on the curve midpoint (DriveRigBones' math family). The
		// midpoint's frame-to-frame world motion kicks the spring opposite the movement, so
		// the wire visibly lags rifle swings and settles -- instead of the rigid re-solved
		// curve the user called out as "solid instead of acting like wires".
		const FVector BaseMid = (Start + End) * 0.5f + SagDir * SagAmount;
		if (!State.bSwayInit) { State.PrevMid = BaseMid; State.bSwayInit = true; }
		const FVector MidDelta = BaseMid - State.PrevMid;
		State.PrevMid = BaseMid;
		// Looser spring + stronger inertia kick than the first pass (spring 90 / kick 0.9
		// settled invisibly fast -- wires read as rigid bent tubes, user feedback twice).
		// Fundamental mode: whole-belly swing.
		State.SwayVel += (-35.f * State.SwayOff) * SimDt - MidDelta * 1.6f;
		State.SwayVel *= FMath::Exp(-4.f * SimDt);
		State.SwayOff += State.SwayVel * SimDt;
		// Second harmonic: S-bend, stiffer/faster, kicked opposite -- makes the cable WHIP
		// rather than arc as one piece.
		State.Sway2Vel += (-70.f * State.Sway2Off) * SimDt + MidDelta * 0.9f;
		State.Sway2Vel *= FMath::Exp(-5.f * SimDt);
		State.Sway2Off += State.Sway2Vel * SimDt;
		// Rigid pipes don't sway; wires sway up to ~1.5x their sag depth.
		const float MaxSway = (State.Type == EKitConnectorType::FluidPipe) ? 0.f
			: FMath::Max(SagAmount * 1.5f, 4.f);
		State.SwayOff = State.SwayOff.GetClampedToMaxSize(MaxSway);
		State.Sway2Off = State.Sway2Off.GetClampedToMaxSize(MaxSway * 0.5f);

		const int32 NumBones = WireBoneNames.Num();   // 8
		auto CurveAt = [&](float T)
		{
			const float W1 = FMath::Sin(PI * T);        // 0 at both anchored ends, peaks mid-wire
			const float W2 = FMath::Sin(2.f * PI * T);  // S-bend: zero at ends AND middle
			return Delta * T + SagDir * (SagAmount * W1) + State.SwayOff * W1 + State.Sway2Off * W2;
		};
		for (int32 b = 0; b < NumBones; ++b)
		{
			const float T0 = float(b) / float(NumBones);
			const float T1 = float(b + 1) / float(NumBones);
			const FVector P0 = CurveAt(T0);
			const FVector SegVec = CurveAt(T1) - P0;
			const float SegLen = SegVec.Size();
			const FVector Dir = SegVec.GetSafeNormal(KINDA_SMALL_NUMBER, FVector::XAxisVector);
			// Swing each bone's ref rotation from its MEASURED ref chain direction onto the
			// live segment direction. Assuming the authoring axis (+X) here was wrong: the
			// FBX conversion can remap axes, and a wrong ref direction rotates the skin
			// sideways off its anchors (in-game: kinked rigid rods not touching the body).
			const FQuat Rot = FQuat::FindBetweenNormals(WireRefDir[b], Dir) * WireRefRot[b];
			// STRETCH each bone along its local chain axis to the live segment length: bone
			// TRANSLATION alone only stretches skin BETWEEN bones -- the tube's tail past the
			// last bone kept its authored 12.5cm regardless of wire span, so on short wires
			// the body-anchor point landed ~7/8 down the wire with a fixed tail flopping past
			// it (user-diagnosed: "middle of the wire attached to the rifle, ends going
			// places"). With the scale, the skin (tail included) spans anchor-to-anchor.
			FVector Sc = WireRefScale[b];
			Sc[WireLocalAxis[b]] *= SegLen / WireRefSegLen;
			Wire->SetBoneTransformByName(WireBoneNames[b],
				FTransform(Rot, P0, Sc), EBoneSpaces::ComponentSpace);
		}
		// Apply the pose THIS frame: SetBoneTransformByName only stores the transform; the
		// skinning refresh normally happens in the poseable's OWN tick, which already ran
		// (we tick TG_PostUpdateWork) -- without this the wire renders one frame late, a
		// visible gap at the anchors while swinging (user-observed; "different tick" was the
		// right diagnosis). Verified against PoseableMeshComponent.cpp.
		Wire->RefreshBoneTransforms();
		Wire->SetVisibility(true);
	}
}

void ALaserRifleWeapon::DriveRigBones(float Dt, const FTransform& RifleXf)
{
	if (!bRigSetup || !RiggedBody || Dt <= 0.f) { return; }
	RigTime += Dt;
	// Clamp the integration step so a frame hitch (or alt-tab catch-up) can't ring the stiff springs.
	const float SimDt = FMath::Min(Dt, 0.04f);
	const float AmpMul  = FMath::Max(0.f, CVarRigAmplitude.GetValueOnGameThread());
	const float DampMul = FMath::Max(0.05f, CVarRigDamping.GetValueOnGameThread());
	const bool  bReactive = CVarRigReactive.GetValueOnGameThread() != 0;
	const bool  bFloppy   = CVarRigFloppy.GetValueOnGameThread() != 0;

	// --- rifle motion expressed in its OWN frame, so behaviour is aim-direction-independent ---
	const FVector Loc = RifleXf.GetLocation();
	const FVector WorldVel = RigVelInit ? (Loc - RigPrevLoc) / Dt : FVector::ZeroVector;
	const FVector WorldAcc = RigVelInit ? (WorldVel - RigPrevVel) / Dt : FVector::ZeroVector;
	RigPrevLoc = Loc; RigPrevVel = WorldVel; RigVelInit = true;
	const FVector LocalAcc = RifleXf.InverseTransformVectorNoScale(WorldAcc);
	const FVector GravDir  = RifleXf.InverseTransformVectorNoScale(FVector(0.f, 0.f, -1.f)); // "down" in comp space

	// Pendant wants to hang along (gravity - accel). Clamp accel so a teleport/respawn can't explode it.
	const float AccScale = bReactive ? 0.0025f : 0.f;
	const FVector Drive = GravDir + (-LocalAcc.GetClampedToMaxSize(6000.f)) * AccScale;
	const float dz = FMath::Max(0.2f, FMath::Abs(Drive.Z));
	// X = fore/aft lean (along barrel +X), Y = side lean (+Y). Small-angle tilt of the hang axis.
	FVector2D PendTarget(FMath::Clamp(Drive.X / dz, -1.2f, 1.2f), FMath::Clamp(Drive.Y / dz, -1.2f, 1.2f));
	PendTarget *= AmpMul;
	// Antenna: light idle sway (+ a touch reactive). Idle persists even when bReactive is off.
	const float idle = RigIdleAmp * AmpMul;
	const FVector2D AntTarget(
		FMath::Sin(RigTime * RigIdleFreq + RigIdlePhase) * idle + (-LocalAcc.X) * AccScale * 0.3f,
		FMath::Cos(RigTime * RigIdleFreq * 0.8f + RigIdlePhase) * idle + (-LocalAcc.Y) * AccScale * 0.3f);

	// --- spring-damper integrate per joint (semi-implicit Euler) ---
	auto Integrate = [&](int32 i, const FVector2D& Target, float StiffMul, float DampMulLocal)
	{
		const FVector2D Acc = (Target - RigAngle[i]) * (RigStiff * StiffMul) - RigAngVel[i] * (RigDamp * DampMul * DampMulLocal);
		RigAngVel[i] += Acc * SimDt;
		RigAngVel[i].X = FMath::Clamp(RigAngVel[i].X, -25.f, 25.f);   // cap so a teleport/hitch can't whip
		RigAngVel[i].Y = FMath::Clamp(RigAngVel[i].Y, -25.f, 25.f);
		RigAngle[i]  += RigAngVel[i] * SimDt;
		RigAngle[i].X = FMath::Clamp(RigAngle[i].X, -1.4f, 1.4f);
		RigAngle[i].Y = FMath::Clamp(RigAngle[i].Y, -1.4f, 1.4f);
	};
	// antenna chain: base reacts, tip lags (floppy whip)
	Integrate(0, AntTarget, 1.0f, 1.0f);
	Integrate(1, bFloppy ? RigAngle[0] * RigLag : FVector2D::ZeroVector, 0.7f, 0.8f);
	// pendant chain: pend_0 swings, pend_1/pend_2 trail with softer springs (the floppy curve).
	// floppy OFF => lower joints add no own-swing, so they inherit pend_0 rigidly (a stiff pendant).
	Integrate(2, PendTarget, 1.0f, 1.0f);
	Integrate(3, bFloppy ? RigAngle[2] * RigLag : FVector2D::ZeroVector, 0.6f, 0.7f);
	Integrate(4, bFloppy ? RigAngle[3] * RigLag : FVector2D::ZeroVector, 0.5f, 0.6f);

	// swing quat in COMPONENT space: X angle about comp Y (fore/aft tilt), Y angle about comp X (side tilt)
	auto SwingQ = [](const FVector2D& A) -> FQuat
	{
		return FQuat(FVector::YAxisVector, A.X) * FQuat(FVector::XAxisVector, A.Y);
	};
	// Walk a chain from the cached root, accumulating component transforms. Each joint rotates about
	// its own head (pivot) in component space; children build on the swung parent frame => bend/whip.
	auto PoseChain = [&](int32 Start, int32 Count)
	{
		FTransform Running = RigRefCompRoot;
		for (int32 k = 0; k < Count; ++k)
		{
			const int32 i = Start + k;
			const FTransform CompNoSwing = RigRefLocal[i] * Running;   // comp = local * parentComp
			const FVector Pivot = CompNoSwing.GetLocation();           // bone head in component space
			const FQuat Q = SwingQ(RigAngle[i]);
			FTransform NewComp;
			NewComp.SetRotation(Q * CompNoSwing.GetRotation());        // rotate orientation about the head
			NewComp.SetLocation(Pivot);                                // head unchanged (pure pivot rotation)
			NewComp.SetScale3D(CompNoSwing.GetScale3D());
			RiggedBody->SetBoneTransformByName(RigBoneNames[i], NewComp, EBoneSpaces::ComponentSpace);
			Running = NewComp;
		}
	};
	PoseChain(0, 2);   // antenna_0 -> antenna_1
	PoseChain(2, 3);   // pend_0 -> pend_1 -> pend_2
	// Apply the pose THIS frame (same reasoning as the connector wires: the poseable's own
	// tick already ran, so without this the rig renders one frame behind the transforms the
	// connector anchors are computed from -- part of the swing-gap the user saw).
	RiggedBody->RefreshBoneTransforms();

	// --- DANGLE LOADOUT TEST (component-swap architecture, lower-risk path per cold review):
	// reuse the ALREADY-COMPUTED pend_2 bone pose via GetSocketTransform (a standard, safe UE
	// query -- NOT the unproven bone-attach API the review flagged) and copy it onto
	// LoadoutComps[0]'s world transform. Proves "does a kit component ride a driven bone" with
	// zero new physics/driver code -- 100% reused math. Scoped test: only slot 0, only while
	// the rig is active (LoadoutComps[1..5] stay on the static-body path, unchanged).
	if (bHasRolledLoadout && LoadoutComps.IsValidIndex(0) && LoadoutComps[0] && LoadoutComps[0]->GetSkeletalMeshAsset()
		&& RigBoneNames.IsValidIndex(4))
	{
		const FTransform PendTipXf = RiggedBody->GetSocketTransform(RigBoneNames[4], RTS_World);
		LoadoutComps[0]->SetWorldLocationAndRotation(PendTipXf.GetLocation(), PendTipXf.GetRotation());
		// Keep whatever self-corrected scale RollRandomComponents already computed -- only
		// position/rotation ride the bone, scale stays as rolled.
	}

	if (CVarRigDebug.GetValueOnGameThread() != 0)
	{
		LR_LOG(CVarLrLogRig,
			TEXT("[LR] Rig acc=(%.0f,%.0f,%.0f) pend=(%.2f,%.2f) p1=(%.2f,%.2f) p2=(%.2f,%.2f) ant=(%.2f,%.2f) react=%d floppy=%d"),
			LocalAcc.X, LocalAcc.Y, LocalAcc.Z, RigAngle[2].X, RigAngle[2].Y, RigAngle[3].X, RigAngle[3].Y,
			RigAngle[4].X, RigAngle[4].Y, RigAngle[0].X, RigAngle[0].Y, bReactive ? 1 : 0, bFloppy ? 1 : 0);
	}
}

void ALaserRifleWeapon::SetupDanglingBattery()
{
	if (bBatteryReady || !BodyMesh) { return; }
	const TSubclassOf<UFGItemDescriptor> BatCls = ResolveBatteryClass();   // cached, fail-open
	UStaticMesh* BatMesh = BatCls ? UFGItemDescriptor::GetItemMesh(BatCls) : nullptr;
	if (!BatMesh)
	{
		// Stop retrying every frame; UpdateDanglingBattery no-ops with the null components.
		bBatteryReady = true;
		UE_LOG(LogLaserRifle, Warning,
			TEXT("[LR] Dangling battery: base-game Battery mesh unresolved (class=%s) -> feature off"),
			*GetNameSafe(BatCls));
		return;
	}
	// Pivot (the hang point that swings) is a child of BodyMesh so it rides the held world transform
	// automatically -- same as the loadout comps. The cell hangs BELOW the pivot and swings about it.
	BatteryPivot = NewObject<USceneComponent>(this, TEXT("BatteryPivot"));
	BatteryPivot->SetupAttachment(BodyMesh);
	BatteryPivot->RegisterComponent();
	BatteryCell = NewObject<UStaticMeshComponent>(this, TEXT("BatteryCell"));
	BatteryCell->SetupAttachment(BatteryPivot);
	BatteryCell->SetStaticMesh(BatMesh);
	BatteryCell->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BatteryCell->SetCastShadow(false);
	BatteryCell->RegisterComponent();
	BatteryCell->SetVisibility(false);
	const FBoxSphereBounds B = BatMesh->GetBounds();
	BatCellLen = 2.f * FMath::Max3(B.BoxExtent.X, B.BoxExtent.Y, B.BoxExtent.Z);
	if (BatCellLen < 1.f) { BatCellLen = 12.f; }
	// Power cables: 2 SK_WireChain poseables the battery terminals wire up to the gun (reuses the
	// connector-wire mesh; posed each frame in UpdateDanglingBattery). Skipped if the wire mesh didn't
	// cook (bWireMeshReady false) -> the battery still hangs + swings, just without visible cables.
	if (bWireMeshReady && WireChainMesh)
	{
		UMaterialInterface* WireMat = WireChainMaterial ? WireChainMaterial.Get()
			: (BodyMaterial ? BodyMaterial.Get() : nullptr);
		for (int32 w = 0; w < 2; ++w)
		{
			UPoseableMeshComponent* Wc = NewObject<UPoseableMeshComponent>(
				this, *FString::Printf(TEXT("BatteryWire_%d"), w));
			Wc->SetupAttachment(BodyMesh);   // world-posed each frame; parent only anchors registration
			Wc->RegisterComponent();
			Wc->SetSkinnedAssetAndUpdate(WireChainMesh);
			if (WireMat) { Wc->SetMaterial(0, WireMat); }
			Wc->SetVisibility(false);
			BatteryWires.Add(Wc);
		}
		BatWireSway.SetNum(BatteryWires.Num());
	}
	bBatteryReady = true;
	LR_LOG(CVarLrLogRig, TEXT("[LR] Dangling battery ready: mesh=%s len=%.1fcm"),
		*GetNameSafe(BatMesh), BatCellLen);
}

void ALaserRifleWeapon::UpdateDanglingBattery(float Dt)
{
	// PARKED 2026-07-05 (user request): with lr.BatteryEnable 0 the feature is fully inert -- don't even
	// create the components; hide any already made (covers a live 1->0 toggle). Set lr.BatteryEnable 1 to
	// bring the whole dangling battery + cables back with no rebuild.
	if (CVarBatteryEnable.GetValueOnGameThread() == 0)
	{
		if (BatteryCell) { BatteryCell->SetVisibility(false); }
		for (UPoseableMeshComponent* Wc : BatteryWires) { if (Wc) { Wc->SetVisibility(false); } }
		bBatVelInit = false;
		return;
	}

	// Lazy Mk1-only setup: build the components the first time a Mk1 rifle ticks with a body present.
	if (!bBatteryReady)
	{
		if (EffectiveMkLevel() != 1) { return; }   // Mk1 frankenrifle only (extend per-tier later)
		SetupDanglingBattery();
	}
	if (!BatteryCell || !BatteryPivot) { return; }

	// --- seat/insert + visibility come straight from the reload state machine ---
	float insertTarget; bool bShow;
	if (bSwapping)
	{
		const float down = FMath::Max(0.01f, SwapDownTime);
		const float up   = FMath::Max(0.01f, SwapUpTime);
		const float t    = RechargeElapsed;
		if (t < down + SwapHoldTime)
		{
			// remove the spent cell: slides OUT as the rifle dips, gone through the hold (empty socket)
			const float u = FMath::Clamp(t / down, 0.f, 1.f);
			insertTarget = 1.f - (u * u * (3.f - 2.f * u));
			bShow = insertTarget > 0.03f;
		}
		else
		{
			// seat a fresh cell as the rifle returns -- the "goes in when reloading" beat
			const float u = FMath::Clamp((t - down - SwapHoldTime) / up, 0.f, 1.f);
			insertTarget = u * u * (3.f - 2.f * u);
			bShow = true;
		}
	}
	else
	{
		// idle: a cell is loaded (CellShots > 0, or < 0 first-use) => seated + shown; empty (== 0, reload
		// failed / out of batteries) => gone -- the "disappears when empty" end state.
		const bool bLoaded = (CellShots != 0.f);
		insertTarget = bLoaded ? 1.f : 0.f;
		bShow = bLoaded;
	}
	bShow = bShow && (EffectiveMkLevel() == 1) && (CVarBatteryEnable.GetValueOnGameThread() != 0);
	BatInsert = insertTarget;
	BatteryCell->SetVisibility(bShow);
	if (!bShow)
	{
		for (UPoseableMeshComponent* Wc : BatteryWires) { if (Wc) { Wc->SetVisibility(false); } }
		bBatVelInit = false;   // re-init velocity tracking on re-show (no accel spike)
		return;
	}

	// --- mount point: body-bounds fraction (taped near the receiver, off the side, below) + CVar nudge ---
	FVector ext(30.f, 12.f, 20.f), ctr(FVector::ZeroVector);
	if (BodyMesh && BodyMesh->GetStaticMesh())
	{
		const FBox Box = BodyMesh->GetStaticMesh()->GetBoundingBox();
		ctr = Box.GetCenter(); ext = Box.GetExtent();
	}
	const FVector MountLocal(
		ctr.X + (-0.10f) * ext.X + CVarBatteryX.GetValueOnGameThread(),
		ctr.Y + ( 0.55f) * ext.Y + CVarBatteryY.GetValueOnGameThread(),
		ctr.Z + (-0.25f) * ext.Z + CVarBatteryZ.GetValueOnGameThread());
	BatteryPivot->SetRelativeLocation(MountLocal);

	// --- pendulum swing: spring-damper reactive to the MOUNT POINT's world motion. Reading the body
	//     ORIGIN (at the hand) missed turning-in-place -- the origin barely moves while the offset mount
	//     sweeps an arc, which is exactly what should throw a hanging battery (that's why it didn't swing).
	//     Softer spring than the rig so the sway is visible + lingers; a touch of idle bob keeps it alive. ---
	BatTime += Dt;
	const float SimDt = FMath::Min(Dt, 0.04f);
	const FTransform BodyXf = BodyMesh ? BodyMesh->GetComponentTransform() : GetActorTransform();
	const FVector MountWorld = BodyXf.TransformPosition(MountLocal);
	const FVector WVel = bBatVelInit ? (MountWorld - BatPrevLoc) / FMath::Max(Dt, 1e-4f) : FVector::ZeroVector;
	const FVector WAcc = bBatVelInit ? (WVel - BatPrevVel) / FMath::Max(Dt, 1e-4f) : FVector::ZeroVector;
	BatPrevLoc = MountWorld; BatPrevVel = WVel; bBatVelInit = true;
	const FVector LocalAcc = BodyXf.InverseTransformVectorNoScale(WAcc);
	const FVector GravDir  = BodyXf.InverseTransformVectorNoScale(FVector(0.f, 0.f, -1.f));
	const float amp = FMath::Max(0.f, CVarBatterySwing.GetValueOnGameThread());
	const FVector Drive = GravDir + (-LocalAcc.GetClampedToMaxSize(5000.f)) * 0.0016f;
	const float dz = FMath::Max(0.2f, FMath::Abs(Drive.Z));
	FVector2D Target(FMath::Clamp(Drive.X / dz, -1.3f, 1.3f), FMath::Clamp(Drive.Y / dz, -1.3f, 1.3f));
	Target.X += FMath::Sin(BatTime * 1.7f) * 0.03f;    // gentle idle bob so a still gun still shows life
	Target.Y += FMath::Cos(BatTime * 1.3f) * 0.03f;
	Target *= amp;
	const FVector2D SAcc = (Target - BatSwing) * 45.f - BatSwingVel * 5.5f;   // softer than the rig -> a real swing
	BatSwingVel += SAcc * SimDt;
	BatSwingVel.X = FMath::Clamp(BatSwingVel.X, -30.f, 30.f);
	BatSwingVel.Y = FMath::Clamp(BatSwingVel.Y, -30.f, 30.f);
	BatSwing += BatSwingVel * SimDt;
	BatSwing.X = FMath::Clamp(BatSwing.X, -1.4f, 1.4f);
	BatSwing.Y = FMath::Clamp(BatSwing.Y, -1.4f, 1.4f);
	// swing about the pivot: X angle about local Y (fore/aft), Y angle about local X (side)
	const FQuat SwingQ = FQuat(FVector::YAxisVector, BatSwing.X) * FQuat(FVector::XAxisVector, BatSwing.Y);
	BatteryPivot->SetRelativeRotation(SwingQ.Rotator());

	// --- the cell hangs below the pivot; slide it down/out along local -Z as it ejects (insert 1->0) ---
	const float autoScl = 25.f / FMath::Max(BatCellLen, 1.f);   // normalize the base-game mesh to ~25cm
	const float scl  = FMath::Max(0.02f, autoScl * FMath::Max(0.02f, CVarBatteryScale.GetValueOnGameThread()));
	const float hang  = BatCellLen * scl * 0.5f;
	const float slide = BatCellLen * scl * 1.2f;
	BatteryCell->SetRelativeLocation(FVector(0.f, 0.f, -hang - (1.f - BatInsert) * slide));
	BatteryCell->SetRelativeScale3D(FVector(scl));

	// --- power cables: SK_WireChain from the battery's top terminals to a fixed plug on the gun body.
	//     Anchored to the RECEIVER (not the movable mount) so the cable spans however far the battery is
	//     dialed out. Reuses PoseWireChainBetween (same sag+sway as the kit wires). ---
	if (bWireMeshReady && BatteryWires.Num() > 0 && BatWireSway.Num() >= BatteryWires.Num())
	{
		const FTransform PivotXf = BatteryPivot->GetComponentTransform();
		const FVector CellWorld  = BatteryCell->GetComponentLocation();
		const FVector PivotLoc   = PivotXf.GetLocation();
		FVector UpToMount = PivotLoc - CellWorld;
		const float cellHalfW = FMath::Max(UpToMount.Size(), 1.f);
		UpToMount /= cellHalfW;                                     // battery -> mount ("up" of the cell)
		const FVector PRight = PivotXf.GetUnitAxis(EAxis::Y);
		const FVector TermTop = CellWorld + UpToMount * cellHalfW;  // top face of the battery (~the terminals)
		const float termSpread = FMath::Max(2.f, 0.30f * cellHalfW);
		const FVector GunPlugLocal(ctr.X - 0.10f * ext.X, ctr.Y + 0.15f * ext.Y, ctr.Z - 0.18f * ext.Z);
		const FVector GunPlug  = BodyXf.TransformPosition(GunPlugLocal);
		const FVector GunRight = BodyXf.GetUnitAxis(EAxis::Y);
		const float gunSpread  = FMath::Max(2.f, 0.12f * ext.Y * FMath::Abs(BodyXf.GetScale3D().Y));
		for (int32 w = 0; w < BatteryWires.Num(); ++w)
		{
			if (!BatteryWires[w]) { continue; }
			const float side = (w == 0) ? 1.f : -1.f;
			const FVector Start = TermTop + PRight * (side * termSpread);
			const FVector End   = GunPlug + GunRight * (side * gunSpread);
			FBatWireSway& S = BatWireSway[w];
			PoseWireChainBetween(BatteryWires[w], Start, End,
				S.Off, S.Vel, S.Off2, S.Vel2, S.PrevMid, S.bInit,
				FMath::Clamp(Dt, 0.001f, 0.04f), false);
		}
	}

	if (CVarBatteryDebug.GetValueOnGameThread() != 0)
	{
		LR_LOG(CVarLrLogRig,
			TEXT("[LR] Battery insert=%.2f show=%d swing=(%.2f,%.2f) mount=(%.1f,%.1f,%.1f) wires=%d cell=%.0f swap=%d"),
			BatInsert, bShow ? 1 : 0, BatSwing.X, BatSwing.Y,
			MountLocal.X, MountLocal.Y, MountLocal.Z, BatteryWires.Num(), CellShots, bSwapping ? 1 : 0);
	}
}

void ALaserRifleWeapon::PoseWireChainBetween(UPoseableMeshComponent* Wire, const FVector& Start, const FVector& End,
	FVector& SwayOff, FVector& SwayVel, FVector& Sway2Off, FVector& Sway2Vel, FVector& PrevMid, bool& bSwayInit,
	float SimDt, bool bTaut)
{
	// Same sag + two-harmonic spring-damper sway as UpdateConnectors' inline kit-wire poser (kept separate
	// so the proven kit path is untouched). Component placed at Start with identity rot/unit scale; bones
	// are world offsets from Start along the curve; each bone stretched to its live segment length.
	if (!Wire || !bWireMeshReady || WireBoneNames.Num() == 0) { return; }
	const FVector Delta = End - Start;
	const float Len = Delta.Size();
	if (Len < 6.f) { Wire->SetVisibility(false); return; }   // too short to be a meaningful cable
	Wire->SetWorldLocationAndRotation(Start, FQuat::Identity);
	Wire->SetWorldScale3D(FVector(1.f));
	const float SagAmount = bTaut ? 0.f : FMath::Clamp(Len * 0.4f, 1.f, 25.f);
	const FVector SagDir = FVector(0.f, 0.f, -1.f);   // WORLD down (real wires hang toward gravity)
	const FVector BaseMid = (Start + End) * 0.5f + SagDir * SagAmount;
	if (!bSwayInit) { PrevMid = BaseMid; bSwayInit = true; }
	const FVector MidDelta = BaseMid - PrevMid;
	PrevMid = BaseMid;
	SwayVel += (-35.f * SwayOff) * SimDt - MidDelta * 1.6f;
	SwayVel *= FMath::Exp(-4.f * SimDt);
	SwayOff += SwayVel * SimDt;
	Sway2Vel += (-70.f * Sway2Off) * SimDt + MidDelta * 0.9f;
	Sway2Vel *= FMath::Exp(-5.f * SimDt);
	Sway2Off += Sway2Vel * SimDt;
	const float MaxSway = bTaut ? 0.f : FMath::Max(SagAmount * 1.5f, 4.f);
	SwayOff = SwayOff.GetClampedToMaxSize(MaxSway);
	Sway2Off = Sway2Off.GetClampedToMaxSize(MaxSway * 0.5f);
	const int32 NumBones = WireBoneNames.Num();
	auto CurveAt = [&](float T)
	{
		const float W1 = FMath::Sin(PI * T);
		const float W2 = FMath::Sin(2.f * PI * T);
		return Delta * T + SagDir * (SagAmount * W1) + SwayOff * W1 + Sway2Off * W2;
	};
	for (int32 b = 0; b < NumBones; ++b)
	{
		const float T0 = float(b) / float(NumBones);
		const float T1 = float(b + 1) / float(NumBones);
		const FVector P0 = CurveAt(T0);
		const FVector SegVec = CurveAt(T1) - P0;
		const float SegLen = SegVec.Size();
		const FVector Dir = SegVec.GetSafeNormal(KINDA_SMALL_NUMBER, FVector::XAxisVector);
		const FQuat Rot = FQuat::FindBetweenNormals(WireRefDir[b], Dir) * WireRefRot[b];
		FVector Sc = WireRefScale[b];
		Sc[WireLocalAxis[b]] *= SegLen / WireRefSegLen;
		Wire->SetBoneTransformByName(WireBoneNames[b], FTransform(Rot, P0, Sc), EBoneSpaces::ComponentSpace);
	}
	Wire->RefreshBoneTransforms();
	Wire->SetVisibility(true);
}

void ALaserRifleWeapon::OnCellRecharged()
{
	// Trigger-agnostic recharge beat: dip down -> HOLD at the bottom (cell swaps) -> return.
	// Hold the fire lockout for the FULL motion so the gun can't fire through the swap. Any
	// recharge source (auto-refill today, battery consumption later) calls this — anim unchanged.
	bSwapping       = true;
	RechargeElapsed = 0.f;
	bSwapMidLogged  = false;
	RechargeFlash   = 1.f;       // existing emissive swell (UpdateHeatFX decays it)
	const float Total = SwapDownTime + SwapHoldTime + SwapUpTime;
	FireCooldown    = FMath::Max(FireCooldown, Total);
	LR_LOG(CVarLrLogFuel,
		TEXT("[LR] OnCellRecharged: swap start (down=%.2f hold=%.2f up=%.2f total=%.2fs)"),
		SwapDownTime, SwapHoldTime, SwapUpTime, Total);
}

void ALaserRifleWeapon::DoReload()
{
	// Refill the internal charge buffer ('cell' = CellShots, the magazine) by spending one 'portion'. When the
	// portions run out, a fresh pack comes from the player's INVENTORY: consume ONE fuel item and set the pack
	// size to THAT fuel's portion count (user 2026-07-12: portions are tied to the fuel). We consume the
	// HIGHEST-portion fuel the player has (the sorted fuel table's first inventory match) -- e.g. a base-game
	// Battery (8) before a cobbled Energy Cell (3): "use the highest available fuel". If NO accepted fuel is
	// present, the reload FAILS (cell stays empty, no swap beat). Inventory writes are authority-only (server is
	// source of truth + replicates); a non-authority caller checks-only so an MP client can't desync the count.
	if (Portions <= 0)
	{
		ResolveFuelTable();   // cached; FuelClasses sorted portions-DESC, fail-open (empty if none resolved)
		UFGInventoryComponent* Inv = nullptr;
		if (AFGCharacterPlayer* Ch = GetInstigatorCharacter()) { Inv = Ch->GetInventory(); }

		if (FuelClasses.Num() > 0 && Inv)
		{
			bool bConsumed = false;
			for (int32 i = 0; i < FuelClasses.Num(); ++i)   // highest-portion first (sorted DESC)
			{
				const TSubclassOf<UFGItemDescriptor> FuelCls = FuelClasses[i];
				const int32 Have = FuelCls ? Inv->GetNumItems(FuelCls) : 0;
				if (Have <= 0) { continue; }
				if (HasAuthority()) { Inv->Remove(FuelCls, 1); }   // authority consumes; clients rely on replication
				PortionsPerBattery = FMath::Max(1, FuelPortions[i]);   // this pack's size (drives the HUD max)
				Portions = PortionsPerBattery;
				bConsumed = true;
				LR_LOG(CVarLrLogFuel,
					TEXT("[LR] Fuel consumed: %s (%d->%d) -> fresh pack of %d portions (highest available)"),
					*GetNameSafe(FuelCls), Have, Have - 1, PortionsPerBattery);
				break;
			}
			if (!bConsumed)
			{
				// The one empty state the fail-open branch does NOT cover: fuel classes resolved fine, but the
				// player carries NONE. If this fires when a cell was never craftable, the Energy Cell recipe-
				// unlock likely didn't cook -- reads like the pre-2026-07-12 brick. Name the recovery loudly.
				UE_LOG(LogLaserRifle, Warning,
					TEXT("[LR] Reload FAILED: no fuel in inventory (cells or Battery) -- craft an Energy Cell at "
					     "the Craft Bench (its recipe unlocks with Laser Rifle Mk1 research). If it is NOT craftable "
					     "there, the Energy Cell recipe-unlock did not cook -- report this."));
				return;   // out of fuel -> no refill, no swap beat
			}
		}
		else
		{
			// No fuel classes resolved OR no inventory -> fail open: refill free so the gun can't brick.
			Portions = PortionsPerBattery;
			UE_LOG(LogLaserRifle, Warning,
				TEXT("[LR] Reload fail-open: %d fuel class(es), inv=%s -> free refill of %d portions"),
				FuelClasses.Num(), Inv ? TEXT("yes") : TEXT("no"), PortionsPerBattery);
		}
	}
	CellShots = ShotsPerCell;
	Portions  = FMath::Max(0, Portions - 1);
	OnCellRecharged();   // cell-swap motion + glow swell + fire lockout
	LR_LOG(CVarLrLogFuel, TEXT("[LR] Reload: %.0f shots, %d portions left"), CellShots, Portions);
}

TSubclassOf<UFGItemDescriptor> ALaserRifleWeapon::ResolveBatteryClass()
{
	// Lazily resolve the mod ENERGY CELL fuel item once. (Was the base-game Battery until 2026-07-12; swapped
	// because the Battery unlocks ~Tier 8, long AFTER the Mk1 rifle -> a fresh early rifle bricked once its
	// starting charge ran out, with no craftable fuel for many tiers. The Energy Cell recipe unlocks WITH the
	// Mk1 rifle schematic, so fuel is always craftable exactly when the rifle is -- see [[laserrifle-energycell]].)
	// FAIL-OPEN + logged: a wrong path must never brick the gun (unresolved -> reloads stay free + no spare
	// count). Member name kept as BatteryItemClass to avoid churn -- it now holds Desc_LR_EnergyCell. Verify
	// the log line shows a class, not FAILED.
	if (!bBatteryResolveTried)
	{
		bBatteryResolveTried = true;
		BatteryItemClass = LoadClass<UFGItemDescriptor>(nullptr,
			TEXT("/LaserRifleMod/Equipment/LaserRifle/Desc_LR_EnergyCell.Desc_LR_EnergyCell_C"));
		LR_LOG(CVarLrLogFuel, TEXT("[LR] Energy Cell item resolve: %s"),
			BatteryItemClass ? *GetNameSafe(BatteryItemClass) : TEXT("FAILED -> reloads free + no spare count (fail-open)"));
	}
	return BatteryItemClass;
}

// THE FUEL TABLE -- the single place fuels are declared. To add a new fuel cell later: author the item, then add
// ONE row here {class path, portions}. DoReload consumes the HIGHEST-portion fuel the player has, so keep this in
// portions-DESCENDING order (ResolveFuelTable also sorts defensively). Portions per item: Battery=8 (dense, late),
// Energy Cell=3 (cheap, early). (user 2026-07-12: portions tied to the fuel; highest available is used.)
namespace
{
	struct FLRFuelRow { const TCHAR* Path; int32 Portions; };
	static const FLRFuelRow LR_FUEL_TABLE[] =
	{
		{ TEXT("/Game/FactoryGame/Resource/Parts/Battery/Desc_Battery.Desc_Battery_C"),        8 },
		{ TEXT("/LaserRifleMod/Equipment/LaserRifle/Desc_LR_EnergyCell.Desc_LR_EnergyCell_C"), 3 },
	};
}

void ALaserRifleWeapon::ResolveFuelTable()
{
	// Lazily load every fuel row once. FAIL-OPEN: a row whose class won't load is SKIPPED (logged), so a bad
	// path can never brick the gun -- an empty table just makes DoReload free-refill. Sorted portions-DESC so
	// DoReload's first inventory match consumes the highest-portion fuel. Verify the log shows each class resolved.
	if (bFuelResolveTried) { return; }
	bFuelResolveTried = true;
	FuelClasses.Reset();
	FuelPortions.Reset();
	for (const FLRFuelRow& Row : LR_FUEL_TABLE)
	{
		if (TSubclassOf<UFGItemDescriptor> Cls = LoadClass<UFGItemDescriptor>(nullptr, Row.Path))
		{
			FuelClasses.Add(Cls);
			FuelPortions.Add(Row.Portions);
			LR_LOG(CVarLrLogFuel, TEXT("[LR] Fuel resolve: %s -> %d portions"), *GetNameSafe(Cls), Row.Portions);
		}
		else
		{
			UE_LOG(LogLaserRifle, Warning, TEXT("[LR] Fuel resolve FAILED (skipped, fail-open): %s"), Row.Path);
		}
	}
	// Defensive descending sort by portions (keeps "highest available" correct even if a row is added out of
	// order). Tiny parallel-array insertion sort.
	for (int32 i = 1; i < FuelClasses.Num(); ++i)
	{
		for (int32 j = i; j > 0 && FuelPortions[j] > FuelPortions[j - 1]; --j)
		{
			FuelPortions.Swap(j, j - 1);
			FuelClasses.Swap(j, j - 1);
		}
	}
	LR_LOG(CVarLrLogFuel, TEXT("[LR] Fuel table resolved: %d fuel(s)"), FuelClasses.Num());
}

int32 ALaserRifleWeapon::CountSpareFuel()
{
	// Sum EVERY accepted fuel item the player carries (cells + Battery) for the HUD "spare" readout.
	ResolveFuelTable();
	if (FuelClasses.Num() == 0) { return -1; }   // nothing resolved -> HUD hides the spare readout
	AFGCharacterPlayer* Ch = GetInstigatorCharacter();
	UFGInventoryComponent* Inv = Ch ? Ch->GetInventory() : nullptr;
	if (!Inv) { return -1; }
	int32 Total = 0;
	for (const TSubclassOf<UFGItemDescriptor>& Cls : FuelClasses)
	{
		if (Cls) { Total += Inv->GetNumItems(Cls); }
	}
	return Total;
}

void ALaserRifleWeapon::EnsureNativeAmmo()
{
	// Re-assert our ammo class after the (closed) vanilla Equip may have cleared it, then
	// build the magazine object so GetAmmoTypeDescriptor() is valid for the HUD/anim code.
	if (mCurrentAmmunitionClass != ULaserRifleAmmo::StaticClass())
	{
		mAllowedAmmoClasses.Empty();
		mAllowedAmmoClasses.Add(ULaserRifleAmmo::StaticClass());
		mDesiredAmmoClass       = ULaserRifleAmmo::StaticClass();
		mCurrentAmmunitionClass = ULaserRifleAmmo::StaticClass();
	}
	// InitializeMagazineObject() lives in the closed game binary; guard so a stub no-op
	// can't loop us. Returns true once a magazine object is built.
	const bool bMag = InitializeMagazineObject();
	bNativeAmmoInit = true;
	LR_LOG(CVarLrLogAmmo,
		TEXT("[LR] NativeAmmo init: ammoClass=%s magObj=%s magSize=%d initMagRet=%d"),
		*GetNameSafe(mCurrentAmmunitionClass), *GetNameSafe(GetAmmoTypeDescriptor()),
		GetMagSize(), bMag ? 1 : 0);
}

void ALaserRifleWeapon::SyncNativeAmmo(float Dt)
{
	// Mirror the live fuel-cell charge into the replicated ammo count the native HUD reads.
	// Only WRITE on change: mCurrentAmmoCount is replicated + SaveGame, so dirtying it every
	// frame would churn the network and fire OnRep_CurrentAmmoCount on remote clients each
	// replication (which the closed game code may react to). Write only on an actual delta.
	const int32 Cell = FMath::Max(0, FMath::FloorToInt(CellShots < 0.f ? ShotsPerCell : CellShots));
	if (mCurrentAmmoCount != Cell) { mCurrentAmmoCount = Cell; }
	// Throttled diagnostic so the log alone shows whether the native source is populated.
	AmmoLogTimer -= Dt;
	if (AmmoLogTimer <= 0.f)
	{
		AmmoLogTimer = 1.0f;
		LR_LOG(CVarLrLogAmmo,
			TEXT("[LR] NativeAmmo sync: GetCurrentAmmo=%d GetMagSize=%d ammoClass=%s magObj=%s spare=%d"),
			GetCurrentAmmo(), GetMagSize(), *GetNameSafe(mCurrentAmmunitionClass),
			*GetNameSafe(GetAmmoTypeDescriptor()),
			GetSpareAmmunition(ULaserRifleAmmo::StaticClass()));
	}
}

void ALaserRifleWeapon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (FireCooldown > 0.f) { FireCooldown -= DeltaSeconds; }

	// lr.RandomComponents test command: any change to the CVar re-rolls the loadout --
	// but ONLY behind the "Random Components (WIP)" session-setting FEATURE FLAG (default
	// OFF). With the flag off the CVar is inert and any previously-rolled loadout is
	// hidden, so the WIP visuals can't leak into normal play (previously a once-set CVar
	// made every fresh equip re-roll via the 0->N edge-detect).
	{
		// PARKED (2026-07-05): the "Random Components (WIP)" session setting was removed (not shown to
		// players). Gate purely on the lr.RandomComponents console CVar now (>0 = on) so a dev can still
		// exercise the WIP loadout; default 0 = off. Re-add the setting + AND it back in when finished.
		const bool bFeatureOn = (CVarRandomComponents.GetValueOnGameThread() > 0);
		if (bFeatureOn)
		{
			const int32 CurRC = CVarRandomComponents.GetValueOnGameThread();
			if (CurRC != LastRandomComponentsValue)
			{
				LastRandomComponentsValue = CurRC;
				RollRandomComponents();
			}
		}
		else if (bHasRolledLoadout)
		{
			// Flag turned off (or was never on): retire the rolled loadout entirely.
			bHasRolledLoadout = false;   // the per-frame visibility blocks hide parts + wires
			LastRandomComponentsValue = CVarRandomComponents.GetValueOnGameThread();
			LR_LOG(CVarLrLogRig, TEXT("[LR] RandomComponents feature flag OFF -> loadout hidden"));
		}
	}

	AFGCharacterPlayer* Char = GetInstigatorCharacter();
	if (!Char) { return; }
	APlayerController* PC = Cast<APlayerController>(Char->GetController());
	if (!PC || !PC->IsLocalController()) { return; }

	const FLinearColor AimColor = bOverheated ? FLinearColor(1.f, 0.15f, 0.1f) : CurrentBeamColor();
	ForceCrosshair(PC);                 // best-effort FG HUD crosshair
	EnsureCrosshair(PC, AimColor);      // our own guaranteed crosshair (red = overheated)
	UpdateHeatFX(DeltaSeconds);
	UpdateSmoke(DeltaSeconds);
	UpdateSparks(DeltaSeconds);
	UpdateArcs(DeltaSeconds);
	UpdatePlasmaOrb(DeltaSeconds);

	// Native ammo HUD: keep the vanilla ammo count mirrored to the fuel-cell charge.
	if (!bNativeAmmoInit) { EnsureNativeAmmo(); }
	SyncNativeAmmo(DeltaSeconds);

	// Always held in hands (procedural). Re-attach if something detached us.
	if (!bAttached) { AttachToArms(); }
	if (bAttached)
	{
		if (bUseArmsAttach) { ProceduralArmsHold(DeltaSeconds); }  // held: hand-follow + aim-forward
		else                { ApplyGripFromConfig(); }            // camera viewmodel
		UpdateConnectors(DeltaSeconds);   // after the hold path so BodyMesh/RiggedBody/parts are fresh this frame
		UpdateDanglingBattery(DeltaSeconds);   // Mk1 dangling battery rides + swings on the fresh BodyMesh transform
	}

	// Live visual refresh: poll the research-driven Mk level and re-apply the look the moment
	// it changes, so buying a Mk schematic upgrades the held rifle WITHOUT a re-equip/save-load.
	VisualPollTimer -= DeltaSeconds;
	if (VisualPollTimer <= 0.f)
	{
		VisualPollTimer = 0.5f;
		const int32 VLnow = EffectiveMkLevel();
		if (VLnow != AppliedVisualLevel) { ApplyVisualsForLevel(VLnow); }
	}

	// Don't fire while a menu/UI is open (MAM, inventory, build menu). Satisfactory shows the
	// mouse cursor whenever a blocking menu is up, so gate the trigger on that — otherwise a
	// click on a research node also fires the laser.
	if (FireCooldown <= 0.f && !bOverheated && !PC->bShowMouseCursor && PC->IsInputKeyDown(EKeys::LeftMouseButton))
	{
		FireLaser(Char, PC);
		// Past the soft limit (Heat>1) the rifle keeps firing but slower, up to
		// FireSlowdownMax x slower at the 2x hard cap.
		const float Over = FMath::Clamp(Heat - 1.f, 0.f, 1.f);
		const float Slow = FMath::Lerp(1.f, GripCfg(LaserRifleSettings::Id_FireSlowdownMax, 5.f), Over);
		// max() so a longer lockout set inside FireLaser (the recharge beat via OnCellRecharged)
		// is NOT clobbered down to the fire interval — otherwise the gun fires through the dip.
		FireCooldown = FMath::Max(FireCooldown, FMath::Max(0.02f, FireInterval * Slow));
	}

	// Manual reload (R): top up the cell early so you're ready for the next fight. Edge-triggered
	// (fires once per press). Skip only if the cell is already full or a swap is already playing.
	// ALLOWED WHILE OVERHEATED (user: on Mk1 you were stuck unable to reload during the long overheat
	// cooldown) -- swapping the energy cell is independent of heat; you still can't FIRE until it cools
	// (FireLaser returns early on bOverheated), but you can reload while waiting.
	const bool bRNow = PC->IsInputKeyDown(EKeys::R) && !PC->bShowMouseCursor;
	if (bRNow && !bReloadKeyWasDown && !bSwapping
		&& CellShots >= 0.f && CellShots < ShotsPerCell)
	{
		LR_LOG(CVarLrLogFuel, TEXT("[LR] Manual reload (R) at %.0f/%.0f"), CellShots, ShotsPerCell);
		DoReload();
	}
	bReloadKeyWasDown = bRNow;
}

void ALaserRifleWeapon::FireLaser(AFGCharacterPlayer* Char, APlayerController* PC)
{
	UWorld* World = GetWorld();
	if (!World || !Char) { return; }
	if (bOverheated) { return; }   // locked out until cooled down

	// --- Energy gate: each shot drains the fuel cell; an empty cell recharges from a battery portion, and
	//     an empty BATTERY now pulls a base-game Battery from the player's inventory (see DoReload). If none
	//     is available the reload fails and the gun can't fire until you carry a Battery. ---
	if (CellShots < 0.f) { CellShots = ShotsPerCell; Portions = PortionsPerBattery; }  // first use
	if (CellShots <= 0.f)
	{
		DoReload();   // empty cell -> auto reload (spend a portion, or consume 1 Battery when the pack is empty)
		return;       // this trigger pull recharges instead of firing
	}
	CellShots -= 1.f;

	const FVector Start = Char->GetCameraComponentWorldLocation();
	const FVector Dir   = Char->GetCameraComponentForwardVector();
	const FVector End   = Start + Dir * Range;

	// Object-type traces (not the Visibility channel): FG creatures are pawns that do NOT block
	// Visibility, so these object types are needed to hit creatures + world.
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams Params(FName(TEXT("LR_Fire")), /*traceComplex*/ false, this);
	Params.AddIgnoredActor(Char);

	// BEAM ENDPOINT = a PRECISE line trace down the exact camera ray (radius 0), so the beam ends
	// where the crosshair points. (User 2026-07-05: the old fat aim-assist SPHERE sweep [r=22] caught
	// near cliff edges / grazing ground BEFORE the crosshair, so the beam landed short -- "the rifle
	// points at the cliff I'm on, not the crosshair beyond". Only diverged on near/grazing shots;
	// far shots matched.) The beam ALWAYS terminates at this crosshair point.
	FHitResult BeamHit;
	const bool bBeamHit = World->LineTraceSingleByObjectType(BeamHit, Start, End, ObjParams, Params);
	const FVector BeamEnd = bBeamHit ? BeamHit.ImpactPoint : End;

	// DAMAGE target = what the beam hit at the crosshair. If that wasn't a creature, keep a SMALL
	// aim-assist: sphere-sweep ONLY as far as the beam reached (bounded, so it can't reach through a
	// wall the beam stopped at) to forgive slight aim on small pawns. This NEVER moves the beam.
	FHitResult Hit = BeamHit;
	bool bHit = bBeamHit;
	const bool bHitPawn = bBeamHit && BeamHit.GetActor() && BeamHit.GetActor()->IsA(APawn::StaticClass());
	if (!bHitPawn)
	{
		FHitResult AssistHit;
		if (World->SweepSingleByObjectType(AssistHit, Start, BeamEnd, FQuat::Identity,
			ObjParams, FCollisionShape::MakeSphere(AimAssistRadius), Params)
			&& AssistHit.GetActor() && AssistHit.GetActor()->IsA(APawn::StaticClass()))
		{
			Hit = AssistHit; bHit = true;   // damage the near creature; beam still ends at BeamEnd
		}
	}

	float Mult = 1.f;
	if (ALaserRifleSubsystem* Sub = GetSub()) { Mult = Sub->GetDamageMultiplier(); }
	// Per-Mk base damage (separate-item design): each Mk rifle is inherently stronger, and the
	// research Damage line (`Mult`) still scales on top. Mk1=1x .. Mk10~7.5x base.
	const float MkScale = FMath::Pow(1.25f, (float)(EffectiveMkLevel() - 1));
	const float Dmg = BaseDamage * MkScale * Mult;

	// Beam from the gun body toward the impact (visible feedback).
	// Muzzle = the +X tip of the body mesh, auto-derived from the CURRENT mesh's
	// local bounds so the beam starts at the barrel for any mesh/scale/grip without
	// a hand-tuned MuzzleOffset (which only matched the old mesh). MuzzleOffset stays
	// as a fallback if the mesh is missing.
	FVector MuzzleLocal = MuzzleOffset;
	if (BodyMesh && BodyMesh->GetStaticMesh())
	{
		const FBox LB = BodyMesh->GetStaticMesh()->GetBoundingBox();
		MuzzleLocal = FVector(LB.Max.X, LB.GetCenter().Y, LB.GetCenter().Z);
	}
	// Per-tier muzzle override (zero = keep the bbox guess). Fixes a tier whose beam exits
	// the wrong end because the export orientation heuristic mis-picked the muzzle.
	{
		const int32 VL = EffectiveMkLevel();
		const int32 MIdx = FMath::Clamp(VL, 1, 10) - 1;
		if (LevelMuzzleOffsets.IsValidIndex(MIdx) && !LevelMuzzleOffsets[MIdx].IsNearlyZero())
		{
			MuzzleLocal = LevelMuzzleOffsets[MIdx];
		}
	}
	// Live tuning nudge (lr.MuzzleDX/DY/DZ, default 0) -- move the beam origin in mesh-local cm to
	// place the emitter exactly, then bake the resulting muzzleLocal (logged in BEAMDIAG) per Mk.
	MuzzleLocal += FVector(CVarMuzzleDX.GetValueOnGameThread(),
		CVarMuzzleDY.GetValueOnGameThread(), CVarMuzzleDZ.GetValueOnGameThread());
	const FVector BeamStart = BodyMesh
		? BodyMesh->GetComponentTransform().TransformPosition(MuzzleLocal)
		: Start;
	ShowBeam(BeamStart, BeamEnd, CurrentBeamColor());
	// Impact splash: a few short electric arcs jumping off the hit point (energy tiers).
	if (bBeamHit && EffectiveMkLevel() >= 4)
	{
		const FLinearColor IC = CurrentBeamColor();
		for (int32 i = 0; i < 3; ++i)
		{
			SpawnArc(BeamEnd, BeamEnd + FMath::VRand() * FMath::FRandRange(12.f, 38.f),
				FMath::FRandRange(0.05f, 0.13f), 0.28f, IC, 3);
		}
	}
	// [LR] BEAM DIAG (strip/quiet before release): per-Mk ground truth for "beam angle off". A
	// path-replacing tier fix that runs through closed engine (hold transform + mesh import axes)
	// can't be proven statically -- this log settles whether an off tier is (a) the body's world
	// FORWARD not level when aiming level (hold/mesh-axis bug), (b) the muzzle-local point off the
	// bore, or (c) just the muzzle-vs-camera offset raking the beam toward a near aim point. Read
	// bodyFwdPitch (should ~= your view pitch) + beamPitch after a few shots per Mk.
	if (BodyMesh)
	{
		const FVector Fwd = BodyMesh->GetForwardVector();
		const FRotator BRot = BodyMesh->GetComponentRotation();
		const FVector BeamDir = (BeamEnd - BeamStart).GetSafeNormal();
		const float CrosshairDist = bBeamHit ? (BeamEnd - Start).Size() : -1.f;   // crosshair hit range (uu), -1 = miss (max range)
		LR_LOG(CVarLrLogBeam,
			TEXT("[LR] BEAMDIAG Mk%d muzzleLocal=(%.1f,%.1f,%.1f) bodyFwdPitch=%.1f bodyRot=(P%.1f Y%.1f R%.1f) beamStart=(%.0f,%.0f,%.0f) beamLen=%.0f crosshairDist=%.0f beamPitch=%.1f viewPitch=%.1f"),
			EffectiveMkLevel(), MuzzleLocal.X, MuzzleLocal.Y, MuzzleLocal.Z,
			FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Fwd.Z, -1.f, 1.f))),
			BRot.Pitch, BRot.Yaw, BRot.Roll,
			BeamStart.X, BeamStart.Y, BeamStart.Z,
			(BeamEnd - BeamStart).Size(), CrosshairDist,
			FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(BeamDir.Z, -1.f, 1.f))),
			FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Dir.Z, -1.f, 1.f))));
	}
	FirePulse = 1.f;   // energy crackle surge through the body this shot
	PulsePos  = 0.f;   // racing-strip band: restart at the strip origin, races to 1 in UpdateHeatFX
	// Laser fire sound at the muzzle (reused base-game zap event). Most static PostEvent
	// helpers are #if 0'd out in the CSS Wwise build; PostAkEventAtLocation is available.
	if (FireSound)
	{
		UAkGameplayStatics::PostAkEventAtLocation(this, FireSound, BeamStart, FRotator::ZeroRotator);
	}

	if (bHit && Hit.GetActor())
	{
		AActor* HitActor = Hit.GetActor();
		const bool bPawn = HitActor && HitActor->IsA(APawn::StaticClass());
		// Damage type MUST be a UFGDamageType subclass (NOT the base UDamageType). Some FactoryGame actors'
		// TakeDamage take the damage event's DamageTypeClass CDO and cast it to UFGDamageType via
		// GetDefaultObject<T>() -- a check()ed cast (Class.h: `check(Ret->IsA(T::StaticClass()))`). AFGSporeFlower
		// does exactly this (FGSporeFlower.cpp:36), so shooting one with a base UDamageType HARD-CRASHED on that
		// assert. We pass ULaserRifleDamageType (our UFGDamageType subclass): the cast succeeds AND its ctor
		// restores the 800 physics impulse + destructible damage that UFGDamageType's ctor zeroes (parity with
		// the pre-fix base-UDamageType feel). Still a UDamageType, so no other TakeDamage handler regresses.
		// See [[laserrifle-sporeflower-crash]].
		UGameplayStatics::ApplyPointDamage(HitActor, Dmg, Dir, Hit,
			PC, this, ULaserRifleDamageType::StaticClass());
		LR_LOG(CVarLrLogBeam, TEXT("[LR] FireLaser HIT %s [%s]%s dmg=%.1f (x%.2f) dmgType=ULaserRifleDamageType"),
			*GetNameSafe(HitActor), *GetNameSafe(Hit.GetComponent()),
			bPawn ? TEXT(" <PAWN>") : TEXT(""), Dmg, Mult);
	}

	// Heat builds per shot. Soft limit = 1.0 (after "Shots to Overheat" shots);
	// you can push to the 2.0 hard cap (firing slows), then it locks until cooled.
	// Overheat is tied to CELL SIZE (user 2026-07-05): soft limit (heat 1.0) at ~half a rapid cell, the 2.0
	// hard-overheat at ~a FULL rapid cell -- so "dump a cell too fast" overheats at every Mk (ShotsPerCell
	// already scales with Mk). The "Shots to Overheat" config is a sensitivity dial (default 6 = x1.0;
	// higher = more forgiving). Heat recovers during reloads + idle (UpdateHeatFX), so paced fire stays cool.
	const float OverheatMult = FMath::Max(0.2f, GripCfg(LaserRifleSettings::Id_OverheatShots, 6.f) / 6.f);
	float Shots = ShotsPerCell * 0.5f * OverheatMult;
	if (ALaserRifleSubsystem* HSub = GetSub())
	{
		// PARKED (2026-07-05): GetHeatTierCount() is 0 (Heat research not in tree) AND Id_HeatPerTier is no
		// longer a registered setting -> guard on the count so we never read the removed option. Re-open
		// (drop the guard) when the Heat Capacity research + its setting return.
		const int32 HeatTiers = HSub->GetHeatTierCount();
		if (HeatTiers > 0) { Shots += (float)HeatTiers * GripCfg(LaserRifleSettings::Id_HeatPerTier, 1.f); }
	}
	Shots = FMath::Max(1.f, Shots);
	Heat = FMath::Min(2.f, Heat + 1.f / Shots);
	LastShotAge = 0.f;   // heat won't cool until the gun is idle again (see UpdateHeatFX)
	if (Heat >= 2.f && !bOverheated) { bOverheated = true;
		LR_LOG(CVarLrLogFx, TEXT("[LR] OVERHEAT (2x) -- locked until cooled to soft limit.")); }
}

void ALaserRifleWeapon::UpdateHeatFX(float DeltaSeconds)
{
	using namespace LaserRifleSettings;
	// Per-shot emissive surge decays back to 0 FAST (was rate 9 ~0.2s -> the muzzle bloom lingered and
	// hid the beam; rate 20 ~0.08s makes the bloom a quick flash so the beam is visible after it).
	FirePulse = FMath::FInterpTo(FirePulse, 0.f, DeltaSeconds, 20.f);
	// Recharge surge: slower swell so a cell-swap reads as a charge-up glow.
	RechargeFlash = FMath::FInterpTo(RechargeFlash, 0.f, DeltaSeconds, 3.f);
	// Cooldown is penalized by how far past the soft limit we pushed: up to
	// OverheatPenalty x slower at the 2x cap (matches "4x cooldown at 2x").
	const float Over = FMath::Clamp(Heat - 1.f, 0.f, 1.f);
	const float Penalty = 1.f + (GripCfg(Id_OverheatPenalty, 4.f) - 1.f) * Over;
	float CoolBonus = 1.f;
	if (ALaserRifleSubsystem* CSub = GetSub())
	{
		// PARKED (2026-07-05): GetCoolTierCount() is 0 (Cooling research not in tree) AND Id_CoolPerTier is
		// no longer a registered setting -> guard on the count so we never read the removed option. Re-open
		// (drop the guard) when the Cooling research + its setting return.
		const int32 CoolTiers = CSub->GetCoolTierCount();
		if (CoolTiers > 0) { CoolBonus += (float)CoolTiers * GripCfg(Id_CoolPerTier, 0.15f); }
	}
	// Mk-scaled (user 2026-07-05): config is the Mk1 BASE (0.4); higher Mk cools faster (~1.5x at Mk10).
	const int32 MkC = FMath::Clamp(EffectiveMkLevel(), 1, 10);
	// User 2026-07-05: LOW Mk cools FASTER (small cells -> should recover through its reload/gaps), high Mk
	// a bit slower. Mk1 = 1.6x base, Mk10 = 1.1x.
	const float MkCoolFactor = FMath::Lerp(1.6f, 1.1f, (float)(MkC - 1) / 9.0f);
	const float Cool = GripCfg(Id_CooldownSpeed, 0.4f) * MkCoolFactor * CoolBonus / FMath::Max(0.01f, Penalty);
	// Heat ONLY cools when the gun is IDLE -- NOT while firing and NOT mid cell-swap. Without this, the long
	// reload swap (~5.8s) + small Mk cells drained all heat between cells so overheat could never build (and
	// at high Mk per-shot gain < decay -> heat fell while firing). Idle-only cooling makes continuous fire
	// always climb to overheat (~2x shots-to-overheat); cooldown just sets how fast you recover once idle.
	LastShotAge += DeltaSeconds;
	// Cool whenever NOT actively firing -- this now INCLUDES the reload swap (user: low Mk should recover
	// through its gaps), just not during rapid continuous fire. Overheat still builds because it's reached
	// WITHIN one rapid cell (see the cell-relative threshold in FireLaser), before the reload gap cools it.
	const bool bIdle = (LastShotAge > 0.30f);
	if (bIdle) { Heat = FMath::Max(0.f, Heat - Cool * DeltaSeconds); }
	if (bOverheated && Heat <= 1.f) { bOverheated = false; }   // back under soft limit -> can fire again

	// Beam brightness scales from "no research" to "full research" by rifle level.
	float Frac = 0.f;
	if (ALaserRifleSubsystem* Sub = GetSub())
	{
		const int32 MaxL = FMath::Max(1, Sub->GetMaxLevel());
		Frac = FMath::Clamp((float)Sub->GetRifleLevel() / (float)MaxL, 0.f, 1.f);
	}
	BeamEmissiveBoost = FMath::Lerp(GripCfg(Id_BeamMin, 10.f), GripCfg(Id_BeamIntensity, 75.f), Frac);

	// Drive the body strips: colour = beam colour, shifting white-hot with heat;
	// brightness scales with heat; tightness controls bleed.
	if (BodyMID)
	{
		const float Gi    = GripCfg(Id_GlowIntensity, 4.f);
		const float Boost = GripCfg(Id_HeatBoost, 1.f);
		const float Thr   = GripCfg(Id_GlowTightness, 0.35f);
		const FLinearColor Base = CurrentBeamColor();
		// At/under the soft limit: normal colour. Past it: blend toward bright red,
		// fully red at the 2x hard cap.
		FLinearColor Hot = (Heat <= 1.f)
			? Base
			: FMath::Lerp(Base, FLinearColor(1.f, 0.f, 0.f), Over);
		// Per-shot flash + recharge swell: blend the strip toward white so they read clearly.
		// FirePulse white-blend kept LOW so the per-shot whole-strip flash doesn't mask the
		// racing band (the band is now the main per-shot visual). RechargeFlash swell unchanged.
		Hot = FMath::Lerp(Hot, FLinearColor::White, FMath::Clamp(FirePulse * 0.05f + RechargeFlash * 0.9f, 0.f, 1.f));
		BodyMID->SetVectorParameterValue(TEXT("GlowColor"), Hot);
		BodyMID->SetScalarParameterValue(TEXT("GlowIntensity"),
			Gi * (1.f + Heat * Boost) + FirePulse * FirePulseAmount + RechargeFlash * (FirePulseAmount * 1.5f));
		BodyMID->SetScalarParameterValue(TEXT("GlowThreshold"), Thr);

		// --- Racing-strip fire FX: a bright band races along the glow strip per shot. ---
		// FireLaser reset PulsePos to 0 last frame; detect that edge for a one-shot diag.
		if (PulsePos < 0.05f && PrevPulsePos >= 1.f)
		{
			LR_LOG(CVarLrLogFx,
				TEXT("[LR] RacingStrip start: band=%.1f sharp=%.1f dur=%.2fs"),
				PulseBandAmount, PulseSharpness, PulseDuration);
		}
		// Advance the band 0 -> 1 over PulseDuration, then park at 1 (band off).
		if (PulsePos < 1.f)
		{
			PulsePos = FMath::Min(1.0f, PulsePos + DeltaSeconds / FMath::Max(0.01f, PulseDuration));
		}
		// Bright additive surge: full intensity while the band travels, off when parked.
		const float Inten = (PulsePos < 1.f) ? PulseBandAmount : 0.f;
		// Harmless no-ops if the material lacks these params (un-recooked material).
		BodyMID->SetScalarParameterValue(TEXT("PulsePos"),       PulsePos);
		BodyMID->SetScalarParameterValue(TEXT("PulseBand"),      Inten);
		BodyMID->SetScalarParameterValue(TEXT("PulseSharpness"), PulseSharpness);
		// Barrel extent so the material maps local X -> 0..1 along the gun (UV-independent band).
		float PulseOriginX = 0.f, PulseLength = 100.f;
		if (BodyMesh && BodyMesh->GetStaticMesh())
		{
			const FBox LB = BodyMesh->GetStaticMesh()->GetBoundingBox();
			PulseOriginX = LB.Min.X;
			PulseLength  = FMath::Max(1.f, LB.Max.X - LB.Min.X);
		}
		BodyMID->SetScalarParameterValue(TEXT("PulseOriginX"), PulseOriginX);
		BodyMID->SetScalarParameterValue(TEXT("PulseLength"),  PulseLength);
		// Surge colour: a strong charged VIOLET, NOT mixed with the beam hue. Mixing emerald+violet
		// gave a washed blue-grey that merged into the cyan glow; a distinct saturated violet contrasts
		// with the glow so the surge reads as its own travelling core. Intensity (PulseBand) is moderated
		// so it stays violet instead of blooming all the way to white.
		const FLinearColor Conc(0.60f, 0.08f, 1.0f, 1.f);
		BodyMID->SetVectorParameterValue(TEXT("PulseColor"), Conc);
		PrevPulsePos = PulsePos;

		CurrentGlow = Hot;
	}
}

void ALaserRifleWeapon::UpdateSmoke(float DeltaSeconds)
{
	using namespace LaserRifleSettings;
	const float Amount = GripCfg(Id_SmokeAmount, 1.f);
	const float StartH = GripCfg(Id_SmokeStartHeat, 0.30f);
	const float Opac   = GripCfg(Id_SmokeOpacity, 0.55f);
	const float Density = FMath::Clamp((Heat - StartH) / FMath::Max(0.01f, 2.f - StartH), 0.f, 1.f);

	if (Amount > 0.f && Density > 0.f && BodyMesh)
	{
		SmokeSpawnTimer -= DeltaSeconds;
		if (SmokeSpawnTimer <= 0.f)
		{
			SpawnPuff(FVector::ZeroVector);   // origin sampled inside, from the body
			SmokeSpawnTimer = FMath::Lerp(0.14f, 0.02f, Density) / FMath::Max(0.1f, Amount);
		}
	}

	AFGCharacterPlayer* Char = GetInstigatorCharacter();
	const FVector CamPos = Char ? Char->GetCameraComponentWorldLocation() : GetActorLocation();

	for (int32 i = 0; i < SmokeComps.Num(); ++i)
	{
		if (!SmokeComps[i] || SmokeAge[i] >= SmokeLife[i]) { continue; }
		SmokeAge[i] += DeltaSeconds;
		const float T = FMath::Clamp(SmokeAge[i] / FMath::Max(0.01f, SmokeLife[i]), 0.f, 1.f);
		if (T >= 1.f) { SmokeComps[i]->SetVisibility(false); continue; }
		const FVector Pos = SmokePos[i] + SmokeVel[i] * SmokeAge[i];
		const float Sc = FMath::Lerp(0.15f, 0.6f, T);
		const FVector ToCam = (CamPos - Pos).GetSafeNormal();       // billboard: plane +Z faces camera
		SmokeComps[i]->SetWorldLocationAndRotation(Pos, FRotationMatrix::MakeFromZ(ToCam).Rotator());
		SmokeComps[i]->SetWorldScale3D(FVector(Sc));
		if (SmokeMIDs.IsValidIndex(i) && SmokeMIDs[i])
		{
			SmokeMIDs[i]->SetVectorParameterValue(TEXT("SmokeColor"), CurrentGlow);
			const float Fade = FMath::Sin(T * PI);                  // 0 -> 1 -> 0
			SmokeMIDs[i]->SetScalarParameterValue(TEXT("SmokeOpacity"), Fade * Opac * (0.4f + 0.6f * Density));
		}
	}
}

void ALaserRifleWeapon::SpawnPuff(const FVector& /*unused*/)
{
	if (!BodyMesh) { return; }
	int32 Slot = INDEX_NONE; float Oldest = -1.f;
	for (int32 i = 0; i < SmokeComps.Num(); ++i)
	{
		if (SmokeAge[i] >= SmokeLife[i]) { Slot = i; break; }
		if (SmokeAge[i] > Oldest) { Oldest = SmokeAge[i]; Slot = i; }
	}
	if (Slot == INDEX_NONE || !SmokeComps[Slot]) { return; }
	// Vent from a random point across the upper body, so smoke rises off the
	// whole gun (over the glow strips), not just the muzzle.
	const FBoxSphereBounds B = BodyMesh->Bounds;
	const FVector Origin = B.Origin + FVector(
		FMath::FRandRange(-0.7f, 0.7f) * B.BoxExtent.X,
		FMath::FRandRange(-0.7f, 0.7f) * B.BoxExtent.Y,
		FMath::FRandRange( 0.1f, 0.7f) * B.BoxExtent.Z);
	SmokePos[Slot] = Origin;
	SmokeVel[Slot] = FVector(FMath::FRandRange(-6.f, 6.f), FMath::FRandRange(-6.f, 6.f), FMath::FRandRange(18.f, 38.f));
	SmokeAge[Slot] = 0.f;
	SmokeLife[Slot] = FMath::FRandRange(0.8f, 1.6f);
	SmokeComps[Slot]->SetVisibility(true);
}

// --- Per-Mk "shinies" -------------------------------------------------------
// Low tiers (1-3, cobbled junk): orange sparks off the exposed bits; on overheat they turn into
// rising flames + heavier sparking. Mid tiers (4-6, industrial): electric-blue crackle between
// parts. High tiers (7-10): mostly the plasma orb, with a few energy motes on fire.
void ALaserRifleWeapon::UpdateSparks(float Dt)
{
	if (!BodyMesh) { return; }
	// RETIRED: the old ember/flame spark system. ALL tiers now use the electric BURST sparks in UpdateArcs
	// (tier-coloured: warm amber at low Mk -> violet at high Mk), so this path emits nothing. The pool +
	// update loop below are kept only so any in-flight sparks fade out cleanly (no fat orange "pills").
	const float Rate = 0.f; const int32 Mode = 0;
	const FLinearColor C(1.f, 0.55f, 0.12f);
	// Spawn the right COUNT this frame (framerate-independent) so bursts read as a spray.
	SparkSpawnTimer += Rate * Dt;
	int32 NSpawn = FMath::Min(FMath::FloorToInt(SparkSpawnTimer), 6);
	SparkSpawnTimer -= (float)NSpawn;
	for (int32 k = 0; k < NSpawn; ++k) { SpawnSpark(C, Mode); }

	for (int32 i = 0; i < SparkComps.Num(); ++i)
	{
		if (!SparkComps[i] || SparkAge[i] >= SparkLife[i]) { continue; }
		SparkAge[i] += Dt;
		const float T = SparkAge[i] / FMath::Max(0.01f, SparkLife[i]);
		if (T >= 1.f) { SparkComps[i]->SetVisibility(false); continue; }
		const bool bElec = (SparkJitter[i] > 0.f);
		// Electrical zigzag: a strong random sideways kick EVERY frame makes the spark dart
		// erratically (the key "electric" tell -- vs a smooth ballistic ember arc).
		if (bElec) { SparkVel[i] += FMath::VRand() * (SparkJitter[i] * Dt); }
		SparkVel[i].Z -= SparkGrav[i] * Dt;                  // gravity (embers fall) / buoyancy (flames rise) / ~none (electric)
		SparkPos[i] += SparkVel[i] * Dt;
		const FVector Dir = SparkVel[i].GetSafeNormal();
		float Wid, Len, Inten;
		if (bElec)
		{
			// bright, length tracks speed (fast dart = longer streak); fast flicker = electric shimmer.
			Wid = 0.0045f;   // a touch thicker than before so the flying sparks actually read
			Len = FMath::Clamp(SparkVel[i].Size() * 0.00013f, 0.008f, 0.06f);
			const float Flick = 0.45f + 0.55f * FMath::Abs(FMath::Sin(SparkAge[i] * 130.f + i * 1.7f));
			Inten = FMath::Lerp(20.f, 0.f, T) * Flick;
		}
		else
		{
			const bool bFlame = (SparkGrav[i] < 0.f);
			Wid = bFlame ? 0.010f : 0.0035f;
			Len = bFlame ? 0.012f : FMath::Lerp(0.022f, 0.005f, T);
			Inten = FMath::Lerp(7.f, 0.f, T);
		}
		SparkComps[i]->SetWorldLocationAndRotation(SparkPos[i],
			FRotationMatrix::MakeFromZ(Dir.IsNearlyZero() ? FVector::UpVector : Dir).Rotator());
		SparkComps[i]->SetWorldScale3D(FVector(Wid, Wid, Len));
		if (SparkMIDs.IsValidIndex(i) && SparkMIDs[i])
		{
			SparkMIDs[i]->SetVectorParameterValue(TEXT("BeamColor"), SparkColor[i]);
			SparkMIDs[i]->SetScalarParameterValue(TEXT("Intensity"), Inten);
		}
	}
}

void ALaserRifleWeapon::SpawnSpark(const FLinearColor& Color, int32 Mode)
{
	if (!BodyMesh || !BodyMesh->GetStaticMesh()) { return; }
	int32 Slot = INDEX_NONE;
	for (int32 i = 0; i < SparkComps.Num(); ++i) { if (SparkAge[i] >= SparkLife[i]) { Slot = i; break; } }
	if (Slot == INDEX_NONE || !SparkComps[Slot]) { return; }
	// Emit from a random point on the FRONT HALF of the gun (local space -> world), where the
	// exposed wires / coils / emitter sit, so the sparks read as coming off the working parts.
	const FBox LB = BodyMesh->GetStaticMesh()->GetBoundingBox();
	const FVector Ext = LB.GetExtent(), Cen = LB.GetCenter();
	const FVector Local = Cen + FVector(
		FMath::FRandRange(0.05f, 1.0f) * Ext.X,              // front half toward the muzzle
		FMath::FRandRange(-0.6f, 0.6f) * Ext.Y,
		FMath::FRandRange(-0.3f, 0.7f) * Ext.Z);
	SparkPos[Slot] = BodyMesh->GetComponentTransform().TransformPosition(Local);
	if (Mode == 1)          // FLAME (overheat, low tiers): slow buoyant rise
	{
		SparkVel[Slot] = FVector(FMath::FRandRange(-10.f, 10.f), FMath::FRandRange(-10.f, 10.f), FMath::FRandRange(25.f, 60.f));
		SparkGrav[Slot] = -60.f;
		SparkLife[Slot] = FMath::FRandRange(0.30f, 0.6f);
		SparkJitter[Slot] = 0.f;
	}
	else if (Mode == 2)     // ELECTRIC: fast random dart, ~no gravity, short flash, strong zigzag
	{
		SparkVel[Slot] = FMath::VRand() * FMath::FRandRange(180.f, 340.f);   // fires in ANY direction from the arc point
		SparkGrav[Slot] = 40.f;                               // barely falls (electricity isn't ballistic)
		SparkLife[Slot] = FMath::FRandRange(0.05f, 0.14f);    // brief flash-and-gone
		SparkJitter[Slot] = FMath::FRandRange(2400.f, 3800.f);// erratic per-frame kick -> zigzag
	}
	else                    // EMBER (low tiers): fast arcing molten spark
	{
		SparkVel[Slot] = FVector(FMath::FRandRange(-90.f, 90.f), FMath::FRandRange(-90.f, 90.f), FMath::FRandRange(20.f, 120.f));
		SparkGrav[Slot] = 300.f;
		SparkLife[Slot] = FMath::FRandRange(0.12f, 0.3f);
		SparkJitter[Slot] = 0.f;
	}
	SparkColor[Slot] = Color;
	SparkAge[Slot] = 0.f;
	SparkComps[Slot]->SetVisibility(true);
}

namespace
{
	struct FBoltSeg { FVector A, B; float W; };   // W = brightness weight (1 = main bolt, <1 = a branch)

	// Recursive BRANCHING lightning (midpoint displacement + forks) -- ported from the Blender-tuned
	// arc_preview_blender.py (dense "Tesla" preset). Writes segments into Out (capped at Cap); re-run
	// every frame for the flicker. Gens=4 dense -> ~34 segs; Gens=3 -> ~12 (small crackle).
	void LR_BuildBolt(const FVector& A, const FVector& B, float Chaos, int32 Gens, bool bBranch,
		float Weight, float MinLen, FBoltSeg* Out, int32& NOut, int32 Cap)
	{
		if (NOut >= Cap) { return; }
		const FVector D = B - A; const float L = D.Size();
		// MinLen = shortest segment we subdivide to. Big for jumps (2cm), tiny for small burst tendrils
		// (~0.1cm) so a sub-centimetre spark still zig-zags instead of collapsing to a straight spike.
		if (Gens <= 0 || L < MinLen) { Out[NOut++] = { A, B, Weight }; return; }
		const FVector U = D / L;
		FVector P1 = FVector::CrossProduct(U, FVector::UpVector);
		if (P1.IsNearlyZero()) { P1 = FVector::CrossProduct(U, FVector::ForwardVector); }
		P1.Normalize();
		const FVector P2 = FVector::CrossProduct(U, P1);
		const float Amp = Chaos * L * 0.5f;
		const FVector Mid = (A + B) * 0.5f + P1 * FMath::FRandRange(-Amp, Amp) + P2 * FMath::FRandRange(-Amp, Amp);
		LR_BuildBolt(A, Mid, Chaos, Gens - 1, bBranch, Weight, MinLen, Out, NOut, Cap);
		LR_BuildBolt(Mid, B, Chaos, Gens - 1, bBranch, Weight, MinLen, Out, NOut, Cap);
		if (bBranch && NOut < Cap && FMath::FRand() < 0.6f)
		{
			const float Ang = (0.35f + FMath::FRand() * 0.55f) * (FMath::FRand() < 0.5f ? 1.f : -1.f);
			const FVector Axis = (FMath::FRand() < 0.5f) ? P1 : P2;
			const FVector BDir = FQuat(Axis, Ang).RotateVector(U);
			const float BLen = L * (0.5f + FMath::FRand() * 0.7f);
			LR_BuildBolt(Mid, Mid + BDir * BLen, Chaos * 1.1f, Gens - 1, FMath::FRand() < 0.35f, Weight * 0.6f, MinLen, Out, NOut, Cap);
		}
	}
}

int32 ALaserRifleWeapon::SpawnArc(const FVector& A, const FVector& B, float Life, float Chaos, const FLinearColor& Color, int32 Gens, const FVector& Vel)
{
	for (int32 a = 0; a < NUM_ARCS; ++a)
	{
		if (ArcAge[a] >= ArcLife[a])   // free slot
		{
			ArcA[a] = A; ArcB[a] = B; ArcVel[a] = Vel; ArcAge[a] = 0.f; ArcLife[a] = Life; ArcChaos[a] = Chaos;
			ArcGens[a] = FMath::Clamp(Gens, 2, 3); ArcColor[a] = Color;
			return a;
		}
	}
	return INDEX_NONE;
}

void ALaserRifleWeapon::UpdateArcs(float Dt)
{
	using namespace LaserRifleSettings;
	if (!BodyMesh || !BodyMesh->GetStaticMesh()) { return; }
	const int32 Tier = EffectiveMkLevel();
	const FBox LB = BodyMesh->GetStaticMesh()->GetBoundingBox();
	const FVector Ext = LB.GetExtent(), Cen = LB.GetCenter();
	const FTransform BX = BodyMesh->GetComponentTransform();
	auto BodyPt = [&](float fx0, float fx1) -> FVector {
		return BX.TransformPosition(Cen + FVector(
			FMath::FRandRange(fx0, fx1) * Ext.X,
			FMath::FRandRange(-0.7f, 0.7f) * Ext.Y,
			FMath::FRandRange(-0.4f, 0.8f) * Ext.Z)); };

	// --- SPAWN (ALL tiers Mk1-10). TWO distinct populations (user: "sparks that float away" vs
	//     "jumps" must read differently): (1) small floating BURST sparks that DRIFT off the front half,
	//     driven by the slider; (2) big branching JUMPS, gated to heat/firing only so the idle/slider look
	//     is sparks, never a big jump. ---
	if (bAttached)
	{
		const float Amount = FMath::Clamp(GripCfg(Id_SparkAmount, 1.0f), 0.f, 3.f);
		const FVector AxX = BX.GetUnitAxis(EAxis::X), AxY = BX.GetUnitAxis(EAxis::Y), AxZ = BX.GetUnitAxis(EAxis::Z);

		// (1) FLOATING SPARKS -- small electric-BURST sparks (a centre with fine tendrils radiating out,
		//     like the reference "spark ball"; built in the render loop as a RADIAL burst, not a directional
		//     bolt). Each pops off the front half, DRIFTS outward + falls, flashes and fades.
		// TIER-COLOURED (user: electric bursts on ALL rifles Mk1-10, coloured by tier): warm amber at low Mk
		// -> violet at high Mk. Each endpoint has ONE dominant channel (amber=R, violet=B) so it survives
		// the tonemap as COLOUR instead of washing to white. Independent of the beam colour.
		const float TierT = FMath::Clamp((Tier - 1) / 9.0f, 0.f, 1.f);
		const FLinearColor SparkCol = FMath::Lerp(FLinearColor(1.0f, 0.40f, 0.08f), FLinearColor(0.45f, 0.08f, 1.0f), TierT);
		// SLIDER = AMOUNT of sparks once FIRING has started (user): there are NO idle sparks at ANY slider
		// value -- sparks are purely fire/heat-driven, so a resting gun never sparks; the slider only scales
		// how many fly while shooting (and briefly after, as the pulse/heat decay). 0 = none even when firing.
		const float SparkRate = (Heat * 18.0f + FirePulse * 34.0f) * Amount;
		ArcSpawnTimer += SparkRate * Dt;
		int32 ns = FMath::Min(FMath::FloorToInt(ArcSpawnTimer), 5);
		ArcSpawnTimer -= (float)ns;
		for (int32 k = 0; k < ns; ++k)
		{
			const FVector PA = BodyPt(0.0f, 1.0f);     // ANYWHERE on the front HALF of the body (not just muzzle)
			// WIDE spread off the gun: up-biased but including backward/toward-player and both sides,
			// not only forward (user: sparks should also fly toward the player).
			const FVector Dir = (AxZ * FMath::FRandRange(0.0f, 1.1f)          // up bias
				+ AxY * FMath::FRandRange(-0.9f, 0.5f)                         // gun-left(view) .. gun-right(player side)
				+ AxX * FMath::FRandRange(-0.8f, 0.6f)                         // backward(toward player) .. forward
				+ FMath::VRand() * 0.5f).GetSafeNormal();
			// B is only used as the BURST RADIUS (|B-A|); render builds tendrils in ALL directions.
			const FVector PB = PA + Dir * FMath::FRandRange(0.30f, 0.75f);    // SMALL burst radius
			const FVector Vel = Dir * FMath::FRandRange(28.f, 68.f);         // drifts away
			SpawnArc(PA, PB, FMath::FRandRange(0.10f, 0.22f), FMath::FRandRange(0.34f, 0.50f), SparkCol, 2, Vel);
		}

		// (2) BIG BRANCHING JUMPS -- anchored bolts jumping off the gun into the air. ONLY when actually
		//     hot or firing; at no-heat idle this rate is 0, so the slider-only look is floating sparks.
		if (Heat > 0.35f || FirePulse > 0.04f)
		{
			const float JumpRate = (Heat * 5.0f + FirePulse * 10.0f) * Amount;
			JumpSpawnTimer += JumpRate * Dt;
			int32 nj = FMath::Min(FMath::FloorToInt(JumpSpawnTimer), 3);
			JumpSpawnTimer -= (float)nj;
			for (int32 k = 0; k < nj; ++k)
			{
				const FVector PA = BodyPt(0.45f, 1.0f);
				FVector PB;
				if (FMath::FRand() < 0.7f)
				{
					const FVector Out = (AxX * FMath::FRandRange(0.0f, 0.6f)
						+ AxY * FMath::FRandRange(-1.0f, -0.1f)
						+ AxZ * FMath::FRandRange(0.1f, 1.0f)).GetSafeNormal();
					PB = PA + Out * FMath::FRandRange(10.f, 26.f);
				}
				else { PB = BodyPt(0.45f, 1.0f); }
				SpawnArc(PA, PB, FMath::FRandRange(0.05f, 0.11f), FMath::FRandRange(0.24f, 0.32f), SparkCol, 3);   // tier-coloured, anchored (Vel=0)
			}
		}

		// (3) GROUND JUMP near overheat: a branching bolt from the emitter to a nearby surface. ---
		GroundJumpTimer -= Dt;
		if (Heat >= 0.85f && Amount > 0.01f && GroundJumpTimer <= 0.f)
		{
			GroundJumpTimer = FMath::FRandRange(0.4f, 1.2f) / FMath::Max(0.25f, Amount);
			const FVector Muzz = BX.TransformPosition(Cen + FVector(Ext.X * 0.9f, 0.f, Ext.Z * 0.2f));
			const FVector Fwd = BX.GetUnitAxis(EAxis::X);
			const FVector Dir = (Fwd + FMath::VRand() * 0.8f - FVector(0, 0, FMath::FRand() * 0.7f)).GetSafeNormal();
			FHitResult H; FCollisionQueryParams Q(FName("LR_ArcJump"), false, this);
			if (AActor* Ins = GetInstigator()) { Q.AddIgnoredActor(Ins); }
			if (UWorld* W = GetWorld())
			{
				if (W->LineTraceSingleByChannel(H, Muzz, Muzz + Dir * 600.f, ECC_WorldStatic, Q))
				{
					SpawnArc(Muzz, H.ImpactPoint, FMath::FRandRange(0.10f, 0.20f), 0.28f,
						FMath::Lerp(SparkCol, FLinearColor(0.9f, 0.95f, 1.0f), 0.3f), 3);   // tier-coloured w/ a hot edge
				}
			}
		}
	}

	// --- RENDER: re-dice every active arc EVERY FRAME (flicker) into a BRANCHING bolt. ---
	FBoltSeg bolt[MAX_SEGS_PER_ARC];
	for (int32 a = 0; a < NUM_ARCS; ++a)
	{
		const int32 base = a * MAX_SEGS_PER_ARC;
		auto HideArc = [&]() { for (int32 s = 0; s < MAX_SEGS_PER_ARC; ++s) if (ArcSegs[base + s]) ArcSegs[base + s]->SetVisibility(false); };
		if (ArcAge[a] >= ArcLife[a]) { HideArc(); continue; }
		ArcAge[a] += Dt;
		const float T = ArcAge[a] / FMath::Max(0.01f, ArcLife[a]);
		if (T >= 1.f) { HideArc(); continue; }
		// FLOATING SPARK: drift the whole little bolt outward + let it fall + slow (air drag), so it
		// reads as a spark that "floats away" rather than an anchored jump. Both endpoints move equally,
		// so the jagged span is preserved and just translates.
		const bool bMicro = !ArcVel[a].IsNearlyZero();
		if (bMicro)
		{
			const FVector Step = ArcVel[a] * Dt;
			ArcA[a] += Step; ArcB[a] += Step;
			ArcVel[a].Z -= 150.f * Dt;                              // gravity: sparks arc downward
			ArcVel[a] *= FMath::Clamp(1.f - 1.8f * Dt, 0.f, 1.f);   // drag: slows as it drifts off
		}
		if ((ArcB[a] - ArcA[a]).SizeSquared() < 0.01f) { HideArc(); continue; }   // allow sub-cm burst sparks
		int32 count = 0;
		if (bMicro)
		{
			// RADIAL BURST: many fine jagged tendrils from the (drifting) origin in random 3D directions
			// -> a small electric spark-ball (matches the reference, not a directional bolt). Re-diced
			// every frame = crackle/shimmer. Loop fills the segment budget so the burst reads dense; the
			// cap bounds it. FilLen (|B-A|, set at spawn) = the burst radius (sub-cm now). MinLen 0.1 so
			// the tiny tendrils still zig-zag.
			const FVector Origin = ArcA[a];
			const float FilLen = FMath::Max(0.12f, (ArcB[a] - ArcA[a]).Size());
			for (int32 f = 0; f < 9 && count < MAX_SEGS_PER_ARC - 2; ++f)
			{
				const FVector Dir = FMath::VRand();
				// start each tendril slightly OUT from the exact centre so 9 tendrils don't pile into one
				// white-hot dot -> the small spark reads as coloured (purple) rather than a white blob.
				const FVector Root = Origin + Dir * FilLen * 0.14f;
				const FVector End = Origin + Dir * FilLen * FMath::FRandRange(0.55f, 1.15f);
				LR_BuildBolt(Root, End, ArcChaos[a], 2, /*branch*/true, 1.0f, 0.1f, bolt, count, MAX_SEGS_PER_ARC);
			}
		}
		else
		{
			LR_BuildBolt(ArcA[a], ArcB[a], ArcChaos[a], ArcGens[a], /*branch*/true, 1.0f, 2.0f, bolt, count, MAX_SEGS_PER_ARC);
		}
		const float Flick = 0.55f + 0.45f * FMath::FRand();   // per-frame brightness flicker
		const float WMain = bMicro ? 0.0026f : 0.00375f;      // jumps 1/4 THINNER (user: 0.005 -> 0.00375)
		const float WBranch = bMicro ? 0.0018f : 0.00225f;
		// LOW intensity keeps HUE (a bright emissive tonemaps to white). Micro sparks are tiny so their
		// whole body is the centre -> keep very dim (5). Jumps were 22 = white; drop to 13 so their
		// tier colour reads while they stay bright enough to be dramatic.
		const float IMax = bMicro ? 5.f : 13.f;
		const float IMin = bMicro ? 1.5f : 2.f;
		for (int32 s = 0; s < MAX_SEGS_PER_ARC; ++s)
		{
			UStaticMeshComponent* Seg = ArcSegs[base + s];
			if (!Seg) { continue; }
			if (s >= count) { Seg->SetVisibility(false); continue; }
			const FVector s0 = bolt[s].A, s1 = bolt[s].B, sd = s1 - s0; const float sl = sd.Size();
			if (sl < 0.02f) { Seg->SetVisibility(false); continue; }
			const float Wid = (bolt[s].W > 0.8f) ? WMain : WBranch;
			Seg->SetWorldLocationAndRotation((s0 + s1) * 0.5f, FRotationMatrix::MakeFromZ(sd / sl).Rotator());
			Seg->SetWorldScale3D(FVector(Wid, Wid, sl / 100.f));
			Seg->SetVisibility(true);
			if (ArcMIDs.IsValidIndex(base + s) && ArcMIDs[base + s])
			{
				ArcMIDs[base + s]->SetVectorParameterValue(TEXT("BeamColor"), ArcColor[a]);
				ArcMIDs[base + s]->SetScalarParameterValue(TEXT("Intensity"), FMath::Lerp(IMax, IMin, T) * Flick * bolt[s].W);
			}
		}
	}
}

void ALaserRifleWeapon::UpdatePlasmaOrb(float Dt)
{
	if (!PlasmaOrb) { return; }
	const int32 Tier = EffectiveMkLevel();
	// DISABLED (user 2026-07-05): the orb used the flat Plane mesh + unlit emissive material with no
	// billboard, so it rendered as a bright white "floating diamond/square" near the muzzle, not an
	// orb. Removed. (A proper version would be a soft-glow sprite/sphere or a Niagara effect.)
	const bool bShow = false;
	if (!bShow) { if (PlasmaOrb->IsVisible()) { PlasmaOrb->SetVisibility(false); } return; }
	OrbPhase += Dt;
	const FBox LB = BodyMesh->GetStaticMesh()->GetBoundingBox();
	const FVector Ext = LB.GetExtent(), Cen = LB.GetCenter();
	const float bob = FMath::Sin(OrbPhase * 3.0f) * 0.06f * Ext.Z;
	const FVector Local(Cen.X + Ext.X * 0.55f, Cen.Y, Cen.Z + Ext.Z * 0.35f + bob);  // near the emitter, above the barrel
	PlasmaOrb->SetWorldLocation(BodyMesh->GetComponentTransform().TransformPosition(Local));
	const float pulse = 0.5f + 0.5f * FMath::Sin(OrbPhase * 6.0f);
	PlasmaOrb->SetWorldScale3D(FVector(0.05f + 0.015f * pulse + FirePulse * 0.05f));
	PlasmaOrb->SetVisibility(true);
	if (PlasmaOrbMID)
	{
		PlasmaOrbMID->SetVectorParameterValue(TEXT("BeamColor"), CurrentBeamColor());
		PlasmaOrbMID->SetScalarParameterValue(TEXT("Intensity"), 6.f + 4.f * pulse + FirePulse * 20.f);
	}
}

void ALaserRifleWeapon::ShowBeam(const FVector& A, const FVector& B, const FLinearColor& Color)
{
	if (!BeamMesh || !BeamMesh->GetStaticMesh())
	{
		static bool bWarned = false;
		if (!bWarned) { bWarned = true;
			UE_LOG(LogLaserRifle, Warning, TEXT("[LR] ShowBeam: no beam mesh (engine cylinder not cooked?) -- firing still works, no visible beam.")); }
		return;
	}
	const FVector Delta = B - A;
	const float Len = Delta.Size();
	if (Len < 1.f) { return; }

	const FVector Mid = (A + B) * 0.5f;
	const FRotator Rot = FRotationMatrix::MakeFromZ(Delta.GetSafeNormal()).Rotator();
	BeamMesh->SetWorldLocationAndRotation(Mid, Rot);
	// Engine cylinder is 100 uu tall, radius 50 -> scale to length and thinness.
	BeamMesh->SetWorldScale3D(FVector(BeamThickness, BeamThickness, Len / 100.f));

	UMaterialInterface* Src = BeamMaterial ? BeamMaterial.Get() : BodyMaterial.Get();
	if (Src)
	{
		if (!BeamMID)
		{
			BeamMID = UMaterialInstanceDynamic::Create(Src, this);
			BeamMesh->SetMaterial(0, BeamMID);
			LR_LOG(CVarLrLogBeam, TEXT("[LR] Beam MID from %s; first colour=(%.2f,%.2f,%.2f)"),
				*GetNameSafe(Src), Color.R, Color.G, Color.B);
		}
		if (BeamMID)
		{
			// Unlit beam material params:
			BeamMID->SetVectorParameterValue(TEXT("BeamColor"), Color);
			BeamMID->SetScalarParameterValue(TEXT("Intensity"), BeamEmissiveBoost);
			// Fallback names (lit body material), harmless if absent:
			BeamMID->SetVectorParameterValue(TEXT("Tint"), Color);
			BeamMID->SetVectorParameterValue(TEXT("EmissiveColor"), Color);
			BeamMID->SetScalarParameterValue(TEXT("EmissiveIntensity"), BeamEmissiveBoost);
		}
	}

	BeamMesh->SetVisibility(true);
	GetWorldTimerManager().SetTimer(BeamHandle, this, &ALaserRifleWeapon::HideBeam, FMath::Max(0.02f, BeamDuration), false);
}

void ALaserRifleWeapon::HideBeam()
{
	if (BeamMesh) { BeamMesh->SetVisibility(false); }
}

void ALaserRifleWeapon::ForceCrosshair(APlayerController* PC)
{
	if (!PC) { return; }
	AFGHUD* HUD = Cast<AFGHUD>(PC->GetHUD());
	if (!HUD) { return; }
	HUD->SetForceHideCrossHair(false);
	HUD->SetShowCrossHair(true);
	HUD->SetCrosshairState(ECrosshairState::ECS_Weapon);
	static bool bLogged = false;
	if (!bLogged) { bLogged = true;
		LR_LOG(CVarLrLogBeam, TEXT("[LR] Crosshair forced ON (ECS_Weapon) via FGHUD.")); }
}

void ALaserRifleWeapon::EnsureCrosshair(APlayerController* PC, const FLinearColor& Color)
{
	if (!PC || !PC->IsLocalController()) { return; }
	if (!Crosshair)
	{
		Crosshair = CreateWidget<ULaserRifleCrosshair>(PC, ULaserRifleCrosshair::StaticClass());
		if (Crosshair)
		{
			Crosshair->CrosshairColor = Color;
			Crosshair->AddToViewport(50);
			bCrosshairAdded = true;
			LR_LOG(CVarLrLogBeam, TEXT("[LR] Custom crosshair widget added to viewport."));
		}
	}
	else
	{
		Crosshair->CrosshairColor = Color;
	}
	// Push the live energy readout to the HUD.
	if (Crosshair)
	{
		Crosshair->CellShots   = FMath::Max(0, FMath::FloorToInt(CellShots < 0.f ? ShotsPerCell : CellShots));
		Crosshair->CellMax     = FMath::Max(1, FMath::FloorToInt(ShotsPerCell));
		Crosshair->Portions    = FMath::Max(0, Portions < 0 ? PortionsPerBattery : Portions);
		Crosshair->PortionsMax = FMath::Max(1, PortionsPerBattery);
		Crosshair->SpareBatteries = CountSpareFuel();   // total spare fuel items (cells + Battery); -1 hides it
		Crosshair->bOverheated = bOverheated;
	}
}

ALaserRifleSubsystem* ALaserRifleWeapon::GetSub() const
{
	if (const UWorld* World = GetWorld())
	{
		if (USubsystemActorManager* Mgr = World->GetSubsystem<USubsystemActorManager>())
		{
			return Mgr->GetSubsystemActor<ALaserRifleSubsystem>();
		}
	}
	return nullptr;
}

int32 ALaserRifleWeapon::EffectiveMkLevel() const
{
	if (FixedMkLevel > 0) { return FMath::Clamp(FixedMkLevel, 1, 10); }
	if (ALaserRifleSubsystem* Sub = GetSub()) { return FMath::Clamp(Sub->GetVisualLevel(), 1, 10); }
	return 1;
}

static FLinearColor LR_PaletteColor(int32 Idx)
{
	static const FLinearColor P[10] = {
		FLinearColor(0.18f, 0.80f, 0.44f), FLinearColor(0.30f, 1.00f, 0.30f),
		FLinearColor(0.20f, 1.00f, 0.80f), FLinearColor(0.20f, 0.85f, 1.00f),
		FLinearColor(0.25f, 0.45f, 1.00f), FLinearColor(0.45f, 0.30f, 1.00f),
		FLinearColor(0.70f, 0.30f, 1.00f), FLinearColor(1.00f, 0.25f, 0.85f),
		FLinearColor(1.00f, 0.35f, 0.15f), FLinearColor(1.00f, 0.95f, 0.80f) };
	return P[FMath::Clamp(Idx, 0, 9)];
}

FLinearColor ALaserRifleWeapon::LevelColor(int32 VisualLevel) const
{
	const int32 Idx = FMath::Clamp(VisualLevel, 1, 10) - 1;
	// Use a BP-authored colour only if it is a real (non-black) override; the BP
	// shipped 10 black entries, which would render an invisible beam.
	if (LevelBeamColors.IsValidIndex(Idx))
	{
		const FLinearColor& C = LevelBeamColors[Idx];
		if (C.R + C.G + C.B > 0.01f) { return C; }
	}
	return LR_PaletteColor(Idx);
}

FLinearColor ALaserRifleWeapon::CurrentBeamColor() const
{
	return LevelColor(EffectiveMkLevel());
}

void ALaserRifleWeapon::RefreshVisuals()
{
	ApplyVisualsForLevel(EffectiveMkLevel());
}

void ALaserRifleWeapon::ApplyVisualsForLevel(int32 VisualLevel)
{
	if (!BodyMesh) { return; }
	const int32 Idx = FMath::Clamp(VisualLevel, 1, 10) - 1;
	AppliedVisualLevel = Idx + 1;   // mark applied so the Tick poll doesn't re-fire every frame

	// Mk-scaled MAGAZINE size only (cell 5->30 shots across Mk1->Mk10). PORTIONS are NOT Mk-scaled anymore --
	// they come from the consumed fuel (see LR_FUEL_TABLE / DoReload), so PortionsPerBattery is left to whatever
	// the last-loaded fuel set (or its starter default). Resize the magazine on a tier change; preserve fill.
	const float MkT = (float)Idx / 9.0f;
	ShotsPerCell = FMath::RoundToFloat(FMath::Lerp(5.f, 30.f, MkT));
	if (CellShots > ShotsPerCell) { CellShots = ShotsPerCell; }

	// Reload cell-swap scales with Mk (user 2026-07-05): ~5.8s at Mk1 (harder, longer downtime) -> 3.0s at
	// Mk10, in-betweens interpolated. Scale all 3 phase-times together (from their Mk1 base) so the swap
	// animation stays in sync with the fire lockout. 3.0/5.8 = 0.517.
	const float SwapScale = FMath::Lerp(1.0f, 3.0f / 5.8f, MkT);
	SwapDownTime = 0.9f * SwapScale;
	SwapHoldTime = 4.0f * SwapScale;
	SwapUpTime   = 0.9f * SwapScale;

	// Rigged-body pilot: Mk1 shows the poseable skeletal body (bones driven each frame); every
	// other tier keeps the static path. CVar lr.RigEnable A/Bs it live (re-checked in the hold).
	const bool bWantRig = (CVarRigEnable.GetValueOnGameThread() != 0) && RiggedMesh && RiggedBody && (Idx == 0);
	bRiggedActive = bWantRig;
	if (RiggedBody) { RiggedBody->SetVisibility(bWantRig); }
	BodyMesh->SetVisibility(!bWantRig);
	// Slot 0 = readback test, slot 1 = true bone-attach test (both rig-only); slots 2-5 = static path.
	for (int32 li = 0; li < LoadoutComps.Num(); ++li)
	{
		USkeletalMeshComponent* L = LoadoutComps[li];
		if (!L) { continue; }
		L->SetVisibility(bHasRolledLoadout && ((li == 0 || li == 1) ? bWantRig : !bWantRig));
	}
	// (the static-mesh assignment below still runs; BodyMesh is just hidden while rigged, so
	//  toggling lr.RigEnable off restores the static look without a re-equip.)
	if (LevelBodyMeshes.IsValidIndex(Idx) && LevelBodyMeshes[Idx])
	{
		BodyMesh->SetStaticMesh(LevelBodyMeshes[Idx]);
		// Per-tier geometry diagnostic (once per tier change). bbox proportions + muzzle (Max.X)
		// + center let per-tier hold scale / muzzle be dialed from the LOG alone, and longestIsX
		// flags any mesh whose barrel didn't land on +X (beam would exit the wrong end).
		if (Idx != LastDiagLevel)
		{
			LastDiagLevel = Idx;
			const FBox LB = LevelBodyMeshes[Idx]->GetBoundingBox();
			const FVector Ext = LB.GetSize();
			const FVector Cen = LB.GetCenter();
			LR_LOG(CVarLrLogVisual,
				TEXT("[LR] Visual Mk%d mesh=%s bbox=(%.1f,%.1f,%.1f) muzzleX=%.1f center=(%.1f,%.1f,%.1f) longestIsX=%d"),
				Idx + 1, *GetNameSafe(LevelBodyMeshes[Idx]), Ext.X, Ext.Y, Ext.Z,
				LB.Max.X, Cen.X, Cen.Y, Cen.Z,
				(Ext.X >= Ext.Y && Ext.X >= Ext.Z) ? 1 : 0);
		}
	}
	// Drive the body's glow strips in the SAME per-level colour as the beam, so
	// they always match. Works on the mesh's own material (M_Rifle_MkNN), which
	// exposes GlowColor/GlowIntensity. A flat BodyMaterial override still works too.
	const FLinearColor C = LevelColor(VisualLevel);
	UMaterialInterface* Src = BodyMaterial ? BodyMaterial.Get() : BodyMesh->GetMaterial(0);
	if (Src)
	{
		UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Src);
		if (!MID)
		{
			MID = UMaterialInstanceDynamic::Create(Src, this);
			BodyMesh->SetMaterial(0, MID);
		}
		BodyMID = MID;   // cached so Tick can drive glow colour/heat live
		if (BodyMID)
		{
			BodyMID->SetVectorParameterValue(TEXT("GlowColor"), C);
			BodyMID->SetScalarParameterValue(TEXT("GlowIntensity"), GlowEmissive);
			BodyMID->SetVectorParameterValue(TEXT("Tint"), C);
			BodyMID->SetVectorParameterValue(TEXT("EmissiveColor"), C);
			BodyMID->SetScalarParameterValue(TEXT("EmissiveIntensity"), EmissiveIntensity);
		}
	}
}
