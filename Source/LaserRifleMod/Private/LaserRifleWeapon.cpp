#include "LaserRifleWeapon.h"
#include "LaserRifleSubsystem.h"
#include "LaserRifleMod.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/CameraComponent.h"
#include "Subsystem/SubsystemActorManager.h"
#include "FGCharacterPlayer.h"
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
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "AkAudioEvent.h"
#include "AkGameplayStatics.h"
#include "TimerManager.h"
#include "InputCoreTypes.h"
#include "UObject/ConstructorHelpers.h"

// Laser fuel-cell ammo descriptor. Magazine size MUST match ShotsPerCell (30) so the
// native ammo HUD shows "<shots left> / 30". Display-only: firing stays custom.
ULaserRifleAmmo::ULaserRifleAmmo()
{
	mMagazineSize = 30;   // protected in UFGAmmoType; matches ALaserRifleWeapon::ShotsPerCell
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
	UE_LOG(LogLaserRifle, Display, TEXT("[LR] FireSound=%s"),
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
		if (Sph.Succeeded()) { S->SetStaticMesh(Sph.Object); }
		S->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		S->SetCastShadow(false);
		S->SetUsingAbsoluteLocation(true); S->SetUsingAbsoluteRotation(true); S->SetUsingAbsoluteScale(true);
		S->SetOnlyOwnerSee(true); S->SetVisibility(false);
			SparkComps.Add(S);
		SparkPos.Add(FVector::ZeroVector); SparkVel.Add(FVector::ZeroVector);
		SparkAge.Add(1.f); SparkLife.Add(1.f); SparkGrav.Add(220.f); SparkColor.Add(FLinearColor::White);
	}
	// Plasma orb (high-tier "shiny"): an emissive sphere that hovers near the emitter.
	PlasmaOrb = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlasmaOrb"));
	PlasmaOrb->SetupAttachment(RootComponent);
	if (Sph.Succeeded()) { PlasmaOrb->SetStaticMesh(Sph.Object); }
	PlasmaOrb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlasmaOrb->SetCastShadow(false);
	PlasmaOrb->SetUsingAbsoluteLocation(true); PlasmaOrb->SetUsingAbsoluteRotation(true); PlasmaOrb->SetUsingAbsoluteScale(true);
	PlasmaOrb->SetOnlyOwnerSee(true); PlasmaOrb->SetVisibility(false);

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
	RefreshVisuals();

	// DIAGNOSTIC: confirm the beam cylinder survived the Shipping cook. If this
	// logs <none>, /Engine/BasicShapes was not packaged -> swap to a cooked mesh.
	UE_LOG(LogLaserRifle, Display, TEXT("[LR] BeginPlay beamMesh=%s bodyMat=%s beamMat=%s colors=%d"),
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
	UE_LOG(LogLaserRifle, Display, TEXT("[LR] Weapon Equip armsAttach=%d armAnim=%d bodyMesh=%s"),
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
	UE_LOG(LogLaserRifle, Display, TEXT("[LR] Body attached to CAMERA viewmodel -- live grip sliders active."));
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
	UE_LOG(LogLaserRifle, Display, TEXT("[LR] Mesh1P sockets: %s"), *Names);

	BodyMesh->AttachToComponent(Arms, FAttachmentTransformRules::SnapToTargetNotIncludingScale, GripSocketName);
	BodyMesh->SetOnlyOwnerSee(true);
	BodyMesh->SetVisibility(true);
	bAttached = true;
	bHeldInit = false;           // re-snap the smoothed hold to the new target
	ProceduralArmsHold(0.f);     // place it immediately (procedural: hand-follow + aim-forward)
	UE_LOG(LogLaserRifle, Display, TEXT("[LR] Body attached to arms socket '%s' (exists=%d) -- PROCEDURAL hold (aim-forward)"),
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
		UE_LOG(LogLaserRifle, Display,
			TEXT("[LR] ARMS-POS cam=%s socket=%s body=%s | body-cam=%.0f socket-cam=%.0f | rel=%s scale=%s vis=%d"),
			*Cam.ToCompactString(), *Sock.ToCompactString(), *Body.ToCompactString(),
			(Body - Cam).Size(), (Sock - Cam).Size(),
			*BodyMesh->GetRelativeLocation().ToCompactString(), *BodyMesh->GetRelativeScale3D().ToCompactString(),
			BodyMesh->IsVisible() ? 1 : 0);
		UE_LOG(LogLaserRifle, Display,
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
	// Baked default placement (your dialed-in values). Always used unless the
	// player opts into the sliders, so it survives SML session-setting resets.
	float Sc = 1.0f;
	FRotator Rot = FRotator::ZeroRotator;
	FVector  Loc(51.f, 10.f, -20.f);
	if (ConfigBool(Id_GripOverride, false))
	{
		Sc  = FMath::Max(0.01f, GripCfg(Id_GripScale, 1.0f));
		Rot = FRotator(GripCfg(Id_GripPitch, 0.f), GripCfg(Id_GripYaw, 0.f), GripCfg(Id_GripRoll, 0.f));
		Loc = FVector(GripCfg(Id_GripX, 51.f), GripCfg(Id_GripY, 10.f), GripCfg(Id_GripZ, -20.f));
	}
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
	if (ConfigBool(Id_GripOverride, false))
	{
		Sc   = FMath::Max(0.01f, GripCfg(Id_GripScale, 0.6f));
		Fine = FRotator(GripCfg(Id_GripPitch, 0.f), GripCfg(Id_GripYaw, 0.f), GripCfg(Id_GripRoll, 0.f));
		Off  = FVector(GripCfg(Id_GripX, 20.f), GripCfg(Id_GripY, 0.f), GripCfg(Id_GripZ, 15.f));
	}
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
			UE_LOG(LogLaserRifle, Display, TEXT("[LR] CellSwap presented: t=%.2f hold=%.2fs pitch=%.1f yawL=%.1f"),
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
	BodyMesh->SetWorldLocationAndRotation(HeldLoc,
		Aim.Quaternion() * Fine.Quaternion() * SwapRot.Quaternion());   // SwapRot = identity when idle
	BodyMesh->SetWorldScale3D(FVector(Sc));
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
	UE_LOG(LogLaserRifle, Display,
		TEXT("[LR] OnCellRecharged: swap start (down=%.2f hold=%.2f up=%.2f total=%.2fs)"),
		SwapDownTime, SwapHoldTime, SwapUpTime, Total);
}

void ALaserRifleWeapon::DoReload()
{
	// Consume a portion, refill the cell to full, play the recharge beat. (Portions auto-refill is a
	// placeholder until the battery-inventory item; manual + empty-cell reloads share this.)
	if (Portions <= 0) { Portions = PortionsPerBattery;
		UE_LOG(LogLaserRifle, Display, TEXT("[LR] Battery empty -> new battery loaded (%d portions)"), Portions); }
	CellShots = ShotsPerCell;
	Portions  = FMath::Max(0, Portions - 1);
	OnCellRecharged();   // cell-swap motion + glow swell + fire lockout
	UE_LOG(LogLaserRifle, Display, TEXT("[LR] Reload: %.0f shots, %d portions left"), CellShots, Portions);
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
	UE_LOG(LogLaserRifle, Display,
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
		UE_LOG(LogLaserRifle, Display,
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
	// (fires once per press). Skip if the cell is already full, a swap is already playing, or
	// while overheated. Consumes a portion like any reload.
	const bool bRNow = PC->IsInputKeyDown(EKeys::R) && !PC->bShowMouseCursor;
	if (bRNow && !bReloadKeyWasDown && !bSwapping && !bOverheated
		&& CellShots >= 0.f && CellShots < ShotsPerCell)
	{
		UE_LOG(LogLaserRifle, Display, TEXT("[LR] Manual reload (R) at %.0f/%.0f"), CellShots, ShotsPerCell);
		DoReload();
	}
	bReloadKeyWasDown = bRNow;
}

void ALaserRifleWeapon::FireLaser(AFGCharacterPlayer* Char, APlayerController* PC)
{
	UWorld* World = GetWorld();
	if (!World || !Char) { return; }
	if (bOverheated) { return; }   // locked out until cooled down

	// --- Energy gate: each shot drains the fuel cell; an empty cell recharges from a
	//     battery portion. (Battery-as-inventory-item comes next; for now portions
	//     auto-refill so the cell+portion mechanic is testable without bricking.) ---
	if (CellShots < 0.f) { CellShots = ShotsPerCell; Portions = PortionsPerBattery; }  // first use
	if (CellShots <= 0.f)
	{
		DoReload();   // empty cell -> auto reload (consume a portion, refill, play the swap beat)
		return;       // this trigger pull recharges instead of firing
	}
	CellShots -= 1.f;

	const FVector Start = Char->GetCameraComponentWorldLocation();
	const FVector Dir   = Char->GetCameraComponentForwardVector();
	const FVector End   = Start + Dir * Range;

	// Sphere sweep over WORLD + PAWN object types: FG creatures are pawns that do
	// NOT block the Visibility line channel, so a plain line trace passed through
	// them and hit the rock behind. Object-type sweep hits creatures + world, and
	// the radius forgives aim on small targets.
	FHitResult Hit;
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams Params(FName(TEXT("LR_Fire")), /*traceComplex*/ false, this);
	Params.AddIgnoredActor(Char);
	const bool bHit = World->SweepSingleByObjectType(Hit, Start, End, FQuat::Identity,
		ObjParams, FCollisionShape::MakeSphere(AimAssistRadius), Params);
	const FVector BeamEnd = bHit ? Hit.ImpactPoint : End;

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
	const FVector BeamStart = BodyMesh
		? BodyMesh->GetComponentTransform().TransformPosition(MuzzleLocal)
		: Start;
	ShowBeam(BeamStart, BeamEnd, CurrentBeamColor());
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
		UGameplayStatics::ApplyPointDamage(HitActor, Dmg, Dir, Hit,
			PC, this, UDamageType::StaticClass());
		UE_LOG(LogLaserRifle, Display, TEXT("[LR] FireLaser HIT %s [%s]%s dmg=%.1f (x%.2f)"),
			*GetNameSafe(HitActor), *GetNameSafe(Hit.GetComponent()),
			bPawn ? TEXT(" <PAWN>") : TEXT(""), Dmg, Mult);
	}

	// Heat builds per shot. Soft limit = 1.0 (after "Shots to Overheat" shots);
	// you can push to the 2.0 hard cap (firing slows), then it locks until cooled.
	float Shots = GripCfg(LaserRifleSettings::Id_OverheatShots, 5.f);
	if (ALaserRifleSubsystem* HSub = GetSub())
	{
		Shots += (float)HSub->GetHeatTierCount() * GripCfg(LaserRifleSettings::Id_HeatPerTier, 1.f);
	}
	Shots = FMath::Max(1.f, Shots);
	Heat = FMath::Min(2.f, Heat + 1.f / Shots);
	if (Heat >= 2.f && !bOverheated) { bOverheated = true;
		UE_LOG(LogLaserRifle, Display, TEXT("[LR] OVERHEAT (2x) -- locked until cooled to soft limit.")); }
}

void ALaserRifleWeapon::UpdateHeatFX(float DeltaSeconds)
{
	using namespace LaserRifleSettings;
	// Per-shot emissive surge decays back to 0 (fast attack from FireLaser, smooth release).
	FirePulse = FMath::FInterpTo(FirePulse, 0.f, DeltaSeconds, 9.f);
	// Recharge surge: slower swell so a cell-swap reads as a charge-up glow.
	RechargeFlash = FMath::FInterpTo(RechargeFlash, 0.f, DeltaSeconds, 3.f);
	// Cooldown is penalized by how far past the soft limit we pushed: up to
	// OverheatPenalty x slower at the 2x cap (matches "4x cooldown at 2x").
	const float Over = FMath::Clamp(Heat - 1.f, 0.f, 1.f);
	const float Penalty = 1.f + (GripCfg(Id_OverheatPenalty, 4.f) - 1.f) * Over;
	float CoolBonus = 1.f;
	if (ALaserRifleSubsystem* CSub = GetSub())
	{
		CoolBonus += (float)CSub->GetCoolTierCount() * GripCfg(Id_CoolPerTier, 0.15f);
	}
	const float Cool = GripCfg(Id_CooldownSpeed, 0.4f) * CoolBonus / FMath::Max(0.01f, Penalty);
	Heat = FMath::Max(0.f, Heat - Cool * DeltaSeconds);
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
			UE_LOG(LogLaserRifle, Display,
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
	const int32 Tier = EffectiveMkLevel();
	const bool bLow = (Tier <= 3);
	const bool bMid = (Tier >= 4 && Tier <= 6);
	float Rate = 0.f; bool bFlame = false;
	FLinearColor C(1.f, 0.55f, 0.12f);          // default warm spark
	if (bLow)
	{
		Rate = 10.f + Heat * 25.f + FirePulse * 90.f;       // many tiny sparks; bursts on fire/heat
		if (Heat >= 1.f) { Rate += 40.f; bFlame = true; C = FLinearColor(1.f, 0.28f, 0.04f); }  // overheat = flames
	}
	else if (bMid)
	{
		C = FLinearColor(0.4f, 0.65f, 1.f);                  // electric blue crackle
		Rate = 4.f + Heat * 14.f + FirePulse * 70.f + (Heat >= 1.f ? 16.f : 0.f);
	}
	else // high tiers: a few energy motes on fire (the orb is the main show)
	{
		C = CurrentBeamColor();
		Rate = FirePulse * 25.f;
	}
	// Spawn the right COUNT this frame (framerate-independent) so bursts read as a spray.
	SparkSpawnTimer += Rate * Dt;
	int32 NSpawn = FMath::Min(FMath::FloorToInt(SparkSpawnTimer), 6);
	SparkSpawnTimer -= (float)NSpawn;
	for (int32 k = 0; k < NSpawn; ++k) { SpawnSpark(C, bFlame); }

	for (int32 i = 0; i < SparkComps.Num(); ++i)
	{
		if (!SparkComps[i] || SparkAge[i] >= SparkLife[i]) { continue; }
		SparkAge[i] += Dt;
		const float T = SparkAge[i] / FMath::Max(0.01f, SparkLife[i]);
		if (T >= 1.f) { SparkComps[i]->SetVisibility(false); continue; }
		SparkVel[i].Z -= SparkGrav[i] * Dt;                  // gravity (sparks fall) / buoyancy (flames rise)
		SparkPos[i] += SparkVel[i] * Dt;
		// TINY STREAK aligned to velocity (thin ellipsoid) = a spark, not a bloomed blob.
		const FVector Dir = SparkVel[i].GetSafeNormal();
		const float Wid = bFlame ? 0.010f : 0.0035f;
		const float Len = bFlame ? 0.012f : FMath::Lerp(0.022f, 0.005f, T);
		SparkComps[i]->SetWorldLocationAndRotation(SparkPos[i],
			FRotationMatrix::MakeFromZ(Dir.IsNearlyZero() ? FVector::UpVector : Dir).Rotator());
		SparkComps[i]->SetWorldScale3D(FVector(Wid, Wid, Len));
		if (SparkMIDs.IsValidIndex(i) && SparkMIDs[i])
		{
			SparkMIDs[i]->SetVectorParameterValue(TEXT("BeamColor"), SparkColor[i]);
			SparkMIDs[i]->SetScalarParameterValue(TEXT("Intensity"), FMath::Lerp(7.f, 0.f, T));
		}
	}
}

void ALaserRifleWeapon::SpawnSpark(const FLinearColor& Color, bool bFlame)
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
	if (bFlame)
	{
		SparkVel[Slot] = FVector(FMath::FRandRange(-10.f, 10.f), FMath::FRandRange(-10.f, 10.f), FMath::FRandRange(25.f, 60.f));
		SparkGrav[Slot] = -60.f;                              // buoyant rise
		SparkLife[Slot] = FMath::FRandRange(0.30f, 0.6f);
	}
	else
	{
		SparkVel[Slot] = FVector(FMath::FRandRange(-90.f, 90.f), FMath::FRandRange(-90.f, 90.f), FMath::FRandRange(20.f, 120.f));
		SparkGrav[Slot] = 300.f;                              // fast arcing sparks
		SparkLife[Slot] = FMath::FRandRange(0.12f, 0.3f);
	}
	SparkColor[Slot] = Color;
	SparkAge[Slot] = 0.f;
	SparkComps[Slot]->SetVisibility(true);
}

void ALaserRifleWeapon::UpdatePlasmaOrb(float Dt)
{
	if (!PlasmaOrb) { return; }
	const int32 Tier = EffectiveMkLevel();
	const bool bShow = (Tier >= 7) && bAttached && BodyMesh && BodyMesh->GetStaticMesh();
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
			UE_LOG(LogLaserRifle, Display, TEXT("[LR] Beam MID from %s; first colour=(%.2f,%.2f,%.2f)"),
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
		UE_LOG(LogLaserRifle, Display, TEXT("[LR] Crosshair forced ON (ECS_Weapon) via FGHUD.")); }
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
			UE_LOG(LogLaserRifle, Display, TEXT("[LR] Custom crosshair widget added to viewport."));
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
			UE_LOG(LogLaserRifle, Display,
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
