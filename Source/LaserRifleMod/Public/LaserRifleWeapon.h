#pragma once

#include "CoreMinimal.h"
#include "Equipment/FGWeapon.h"
#include "LaserRifleWeapon.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class ALaserRifleSubsystem;
class ULaserRifleCrosshair;
class AFGCharacterPlayer;
class UAkAudioEvent;

/**
 * Minimal ammo descriptor for the laser's fuel cell. We do NOT route firing through
 * the vanilla ammo pipeline (the rifle keeps its custom Tick-poll trace + energy logic);
 * this exists only so AFGWeapon has a valid mCurrentAmmunitionClass, which is what the
 * game's NATIVE ammo HUD widget reads (GetCurrentAmmo / GetMagSize). MagazineSize must
 * match ShotsPerCell so the native readout reads "cell shots / cell capacity".
 */
UCLASS()
class LASERRIFLEMOD_API ULaserRifleAmmo : public UFGAmmoType
{
	GENERATED_BODY()
public:
	ULaserRifleAmmo();
};

/**
 * Laser Rifle weapon: camera-anchored viewmodel body + custom Tick-polled firing
 * (trace from view, subsystem-scaled damage) + an emissive beam mesh shown per
 * shot (works in Shipping where debug draw is stripped). Per-Mk body mesh and
 * beam color come from the subsystem's GetVisualLevel().
 */
UCLASS()
class LASERRIFLEMOD_API ALaserRifleWeapon : public AFGWeapon
{
	GENERATED_BODY()

public:
	ALaserRifleWeapon();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void Equip(class AFGCharacterPlayer* character) override;
	virtual void UnEquip() override;

	UFUNCTION(BlueprintCallable, Category = "LaserRifle")
	void RefreshVisuals();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LaserRifle")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** Thin emissive cylinder used as the laser beam (shown briefly per shot). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LaserRifle")
	TObjectPtr<UStaticMeshComponent> BeamMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LaserRifle")
	TArray<TObjectPtr<UStaticMesh>> LevelBodyMeshes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LaserRifle")
	TArray<FLinearColor> LevelBeamColors;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LaserRifle")
	TObjectPtr<UMaterialInterface> BodyMaterial;

	/** Unlit emissive material used for the beam (M_LaserRifle_Beam). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LaserRifle")
	TObjectPtr<UMaterialInterface> BeamMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LaserRifle")
	float EmissiveIntensity = 5.0f;

	/** Glow strength for the body's energy strips (driven in the per-level colour). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LaserRifle")
	float GlowEmissive = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "LaserRifle|Viewmodel")
	FVector ViewmodelLocation = FVector(50.f, 15.f, -18.f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "LaserRifle|Viewmodel")
	FRotator ViewmodelRotation = FRotator(0.f, 0.f, 0.f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "LaserRifle|Viewmodel")
	float ViewmodelScale = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "LaserRifle|Fire")
	float BaseDamage = 10.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "LaserRifle|Fire")
	float FireInterval = 0.15f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "LaserRifle|Fire")
	float Range = 12000.f;

	/** Muzzle point in BodyMesh local space; the beam originates here. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "LaserRifle|Fire")
	FVector MuzzleOffset = FVector(60.f, 0.f, 0.f);

	// --- Route 2: hold the rifle in the first-person arms (AE_Rifle pose) ---
	/** true = attach to the arm hand socket (hands grip it); false = camera viewmodel. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "LaserRifle|Grip")
	bool bUseArmsAttach = true;   // default: held in hands. "Hold in Hands" off = opt out.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "LaserRifle|Grip")
	FName GripSocketName = TEXT("weaponSocket");
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "LaserRifle|Grip")
	FVector GripLocation = FVector(0.f, 0.f, 0.f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "LaserRifle|Grip")
	FRotator GripRotation = FRotator(0.f, 0.f, 0.f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "LaserRifle|Grip")
	float GripScale = 1.0f;

	// Procedural held-hold state (arms mode): smoothed world position that follows the hand.
	FVector HeldLoc = FVector::ZeroVector;
	bool bHeldInit = false;

	/** Sphere-sweep radius for forgiving aim + reliable creature hits (uu). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "LaserRifle|Fire")
	float AimAssistRadius = 22.f;

	/** Beam thickness (cylinder X/Y scale) and how long each beam flash lasts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "LaserRifle|Fire")
	float BeamThickness = 0.04f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "LaserRifle|Fire")
	float BeamDuration = 0.05f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "LaserRifle|Fire")
	float BeamEmissiveBoost = 50.f;

	/** Per-shot emissive surge added to the body glow (energy crackle; no recoil). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "LaserRifle|FX")
	float FirePulseAmount = 9.0f;

	/** Racing-strip band brightness pushed to the material while a shot's pulse travels.
	 *  High so the white streak reads over the per-shot glow flash + bloom. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "LaserRifle|FX")
	float PulseBandAmount = 120.0f;  // bright surge, moderated so the violet holds (160 bloomed to white)
	/** Racing-strip band width (higher = NARROWER). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "LaserRifle|FX")
	float PulseSharpness = 13.0f;

	// --- Cell-swap "present and work the cell" pose (view space; cm / degrees). The rifle
	//     stays OUT FRONT and visible, muzzle tilts up + swings left, with a working wobble. ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "LaserRifle|CellSwap")
	float SwapTiltPitch  = 22.0f;   // muzzle tilts UP while presenting (deg)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "LaserRifle|CellSwap")
	float SwapYawLeft    = 30.0f;   // muzzle swings across to the LEFT (deg)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "LaserRifle|CellSwap")
	float SwapTiltRoll   = 10.0f;   // roll so it reads as a hand turn (deg)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "LaserRifle|CellSwap")
	float SwapPresentFwd = 5.0f;    // push slightly forward so the gun stays in view (cm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "LaserRifle|CellSwap")
	float SwapPresentUp  = 6.0f;    // lift so the gun sits up in view, not retreated (cm)
	/** Seconds the rifle holds dipped at the bottom while the cell swaps (a future research
	 *  line can shorten this for a faster reload). Down/up times keep the dip's in/out speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "LaserRifle|CellSwap")
	float SwapHoldTime = 4.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "LaserRifle|CellSwap")
	float SwapDownTime = 0.9f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "LaserRifle|CellSwap")
	float SwapUpTime   = 0.9f;

	/** Wwise event played at the muzzle on each shot (a zappy energy-weapon sound). */
	UPROPERTY(EditDefaultsOnly, Category = "LaserRifle|FX")
	TObjectPtr<UAkAudioEvent> FireSound = nullptr;

private:
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BodyMID = nullptr;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BeamMID = nullptr;

	// --- Heat smoke/steam puffs (pooled translucent spheres) ---
	UPROPERTY(EditDefaultsOnly, Category = "LaserRifle|FX")
	TObjectPtr<UMaterialInterface> SmokeMaterial = nullptr;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> SmokeComps;
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> SmokeMIDs;
	TArray<FVector> SmokePos;
	TArray<FVector> SmokeVel;
	TArray<float> SmokeAge;
	TArray<float> SmokeLife;
	float SmokeSpawnTimer = 0.f;
	FLinearColor CurrentGlow = FLinearColor::White;

	float FireCooldown = 0.f;
	bool bAttached = false;
	float Heat = 0.f;          // 0..1 heat; >=1 = overheated (locked until cooled)
	bool bOverheated = false;
	float FirePulse = 0.f;     // 0..1 per-shot emissive surge (energy crackle on fire)

	// --- Racing-strip fire FX: a bright band races along the glow strip per shot ---
	float PulsePos      = 1.0f;   // 0..1 band position; >=1 = parked (no active band)
	float PulseDuration = 0.30f;  // seconds for the band to race 0 -> 1 per shot (faster, snappier surge)
	float PrevPulsePos  = 1.0f;   // edge-detect for the one-shot-per-fire diag (member, not static)

	// --- Cell-swap recharge motion (plays on OnCellRecharged, trigger-agnostic) ---
	bool  bSwapping       = false; // recharge cell-swap motion currently playing
	float RechargeElapsed = 0.f;   // seconds since the swap started (drives down/hold/up phases)
	bool  bSwapMidLogged  = false; // one-shot "reached the bottom" diag latch (member, not static)
	bool  bReloadKeyWasDown = false; // R-key edge detect for the manual reload

	// --- Energy: fuel cell (shots) recharged from battery portions ---
	float ShotsPerCell = 30.f;          // a full cell = this many shots
	int32 PortionsPerBattery = 8;       // a battery holds this many cell-recharges
	float CellShots = -1.f;             // shots left in the cell (-1 = fill on first use)
	int32 Portions  = -1;               // recharge portions left in the battery
	float RechargeFlash = 0.f;          // brief visual surge when a cell recharges
	float AmmoLogTimer = 0.f;           // throttle for the native-ammo diagnostic log
	bool  bNativeAmmoInit = false;      // one-time native ammo-class wiring done
	FTimerHandle BeamHandle;

	/** Set mCurrentAmmunitionClass / desired / mag object so the native ammo HUD reads us. */
	void EnsureNativeAmmo();
	/** Push the live cell charge into mCurrentAmmoCount each frame (native HUD source). */
	void SyncNativeAmmo(float Dt);

	UPROPERTY()
	TObjectPtr<ULaserRifleCrosshair> Crosshair = nullptr;
	bool bCrosshairAdded = false;

	void ApplyVisualsForLevel(int32 VisualLevel);
	void AttachToCamera();
	void AttachToArms();
	void ApplyGripFromConfig();
	/** Arms mode: position the rifle at the hand (follows arm anim) but orient it along
	 *  the player's aim (always forward, stable). Math-driven; immune to the hand bone's
	 *  unstable rotation. Offset/rotation sliders fine-tune in view space. */
	void ProceduralArmsHold(float Dt);
	/** Start the cell-swap motion + glow swell + fire lockout. Call from ANY recharge
	 *  source (auto-refill today, battery-inventory consumption later) — trigger-agnostic. */
	void OnCellRecharged();
	/** Consume a portion, refill the cell to full, and play the recharge beat. Shared by the empty-cell
	 *  auto-reload and the manual R-key reload. */
	void DoReload();
	void UpdateHeatFX(float DeltaSeconds);
	void UpdateSmoke(float DeltaSeconds);
	void SpawnPuff(const FVector& Origin);
	float GripCfg(const TCHAR* StrId, float DefaultValue) const;
	bool  ConfigBool(const TCHAR* StrId, bool DefaultValue) const;
	void FireLaser(AFGCharacterPlayer* Char, class APlayerController* PC);
	void ShowBeam(const FVector& A, const FVector& B, const FLinearColor& Color);
	void HideBeam();
	void ForceCrosshair(class APlayerController* PC);
	void EnsureCrosshair(class APlayerController* PC, const FLinearColor& Color);
	FLinearColor LevelColor(int32 VisualLevel) const;
	ALaserRifleSubsystem* GetSub() const;
	FLinearColor CurrentBeamColor() const;
};
