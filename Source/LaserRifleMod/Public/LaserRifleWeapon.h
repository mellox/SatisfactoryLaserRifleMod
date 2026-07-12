#pragma once

#include "CoreMinimal.h"
#include "Equipment/FGWeapon.h"
#include "DamageTypes/FGDamageType.h"
#include "LaserRifleWeapon.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class USceneComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class ALaserRifleSubsystem;
class ULaserRifleCrosshair;
class AFGCharacterPlayer;
class UFGItemDescriptor;
class UFGInventoryComponent;
class UAkAudioEvent;
class UPoseableMeshComponent;
class USkeletalMesh;

/** How a kit-component's connector to the body should be drawn. Inferred from the part's
 *  asset name at roll time (see InferConnectorSpecs) -- no per-part authoring required. */
enum class EKitConnectorType : uint8
{
	None,           // part sits flush/mounted directly on the body -- no visible connector.
	StructuralWire, // thin connector (generic mounting/signal/stabilizing).
	PowerWire,      // like StructuralWire but for power/circuit parts (battery/core/capacitor).
	FluidPipe,      // thicker connector (coolant/fluid-line parts).
};

/** One connector anchor a kit part needs. LocalOffset is in the PART's own local space --
 *  since the skeletal-pool pass, it is the REF-POSE POSITION OF AN AUTHORED conn_N BONE on
 *  the part's own skeleton (rigged in Blender, batch-exported to fbx_rigged/), not a bbox
 *  guess. BoneName records which bone it came from (diagnostics). */
struct FKitConnectorSpec
{
	EKitConnectorType Type = EKitConnectorType::StructuralWire;
	FVector LocalOffset = FVector::ZeroVector;
	FName BoneName;
};

/** Live per-slot connector state: which part it's anchored to and where, re-evaluated every
 *  frame in UpdateConnectors() -- NOT baked/simulated. History of the rendering mechanism:
 *  (1) UCableComponent Verlet rope physics -- FAILED: assumes a smoothly-moving attach point;
 *  this rifle's viewmodel hold is an INSTANT snap-to-aim every frame (ProceduralArmsHold,
 *  "no smoothing" by design), which the sim read as near-infinite velocity and whipped the
 *  wires toward the horizon. (2) 3 stretched rigid cylinder segments -- worked but read as
 *  disjointed rods, not one wire, and tracking a bone-attached part's component transform was
 *  one frame stale (user-visible connector lag while the part swung). (3) USplineMeshComponent
 *  bent between the anchors -- looked worse in-game (fat bent tubes). (4) CURRENT, per the
 *  user's explicit direction: a REAL skeleton -- SK_WireChain, a thin skinned tube with an
 *  8-bone chain, posed along the live sag curve every frame in UpdateConnectors() via
 *  SetBoneTransformByName (the same mechanism DriveRigBones uses for the rifle's dangling
 *  battery). Still zero persisted simulation state. */
struct FKitConnectorState
{
	TWeakObjectPtr<USkeletalMeshComponent> PartComp;
	FVector PartLocalOffset = FVector::ZeroVector;   // in PartComp's local space
	FVector BodyLocalOffset = FVector::ZeroVector;   // in BodyMesh's local space
	FName PartBoneName;                              // the part's conn_N bone the wire is ATTACHED to
	FName BodyBoneName;                              // the body's mount_N bone (rigged body path)
	EKitConnectorType Type = EKitConnectorType::StructuralWire;
	int32 OwnerMountIndex = INDEX_NONE;              // LoadoutComps index whose visibility this mirrors
	bool bActive = false;
	// Live wire sway (spring-damper on the curve midpoint, same math family as DriveRigBones'
	// pendulum): the wire lags rifle motion and settles, instead of being a rigid re-solved
	// curve every frame ("solid instead of acting like wires", user-observed).
	FVector SwayOff = FVector::ZeroVector;
	FVector SwayVel = FVector::ZeroVector;
	FVector Sway2Off = FVector::ZeroVector;   // second harmonic (S-bend) for whippier cable
	FVector Sway2Vel = FVector::ZeroVector;
	FVector PrevMid = FVector::ZeroVector;
	bool bSwayInit = false;
};

/** Spring-damper sway state for one dangling-battery power cable (same math family as the kit
 *  connector wires). Plain runtime state -- not persisted/replicated. */
struct FBatWireSway
{
	FVector Off = FVector::ZeroVector, Vel = FVector::ZeroVector;
	FVector Off2 = FVector::ZeroVector, Vel2 = FVector::ZeroVector;
	FVector PrevMid = FVector::ZeroVector;
	bool bInit = false;
};

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
 * Laser damage type. Subclasses UFGDamageType so FactoryGame actors whose TakeDamage casts the damage event's
 * DamageTypeClass CDO to UFGDamageType via GetDefaultObject<T>() (a check()ed cast -- e.g. AFGSporeFlower,
 * FGSporeFlower.cpp:36) do NOT crash the way a base UDamageType did. UFGDamageType's ctor ZEROES
 * DamageImpulse/DestructibleImpulse (base UDamageType uses 800), so we restore them + mShouldDamageDestructible
 * to keep the knockback / destructible behaviour the laser had before the crash fix. See [[laserrifle-sporeflower-crash]].
 */
UCLASS()
class LASERRIFLEMOD_API ULaserRifleDamageType : public UFGDamageType
{
	GENERATED_BODY()
public:
	ULaserRifleDamageType(const FObjectInitializer& ObjectInitializer);
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

	/** Poseable skeletal body for rifles with a rigged mesh (Mk1 pilot). Driven world-absolute
	 *  exactly like BodyMesh, but its bones (antenna sway + reactive dangling-battery pendulum +
	 *  floppy wires) are posed each frame in DriveRigBones. Only shown when RiggedMesh is set,
	 *  the rifle is Mk1, AND CVar lr.RigEnable is on; otherwise the static BodyMesh shows. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LaserRifle")
	TObjectPtr<UPoseableMeshComponent> RiggedBody;

	/** Separate-item design: if >0, THIS equipment IS that Mk (its descriptor is one of the 10
	 *  craftable Mk rifles) and its look/stats are fixed to this Mk. 0 = legacy single-item that
	 *  follows research level. Each BP_Equip_LaserRifle_MkN child sets this to N. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LaserRifle")
	int32 FixedMkLevel = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LaserRifle")
	TArray<TObjectPtr<UStaticMesh>> LevelBodyMeshes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LaserRifle")
	TArray<FLinearColor> LevelBeamColors;

	/** Pool of standalone kit-component SKELETAL meshes (batteries/antennas/vents/etc.) that
	 *  lr.RandomComponents draws 6 distinct entries from at runtime. Each part carries its own
	 *  skeleton: a rigid root bone + conn_0[/conn_1] anchor bones marking where connector
	 *  wires plug in (per the user's skeletons direction). Imported by
	 *  Scripts/ue/import_kit_pool_skeletal.py; CDO populated by a headless Python step after
	 *  the C++ build (two-build sequence, same as FixedMkLevel). Empty = feature no-ops. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LaserRifle|Loadout")
	TArray<TObjectPtr<USkeletalMesh>> KitComponentPool;

	/** Per-tier first-person HOLD scale override (index 0 = Mk1). <=0 or missing = use the
	 *  default 0.6. Lets each Mk's mesh be sized in-hand without re-exporting; the per-tier
	 *  geometry log ([LR] Visual MkN bbox=...) gives the numbers to fill this. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "LaserRifle|PerTier")
	TArray<float> LevelHoldScales;

	/** Per-tier muzzle point in BodyMesh local space (index 0 = Mk1). ZERO/missing = use the
	 *  mesh bbox Max.X. Use this if a tier's beam exits the wrong end (orientation heuristic
	 *  guessed the muzzle wrong for that mesh). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "LaserRifle|PerTier")
	TArray<FVector> LevelMuzzleOffsets;

	/** Per-tier HELD-orientation correction (Pitch,Yaw,Roll deg), index 0 = Mk1, applied in the
	 *  mesh's LOCAL space on top of the aim. Some bodies' sculpted barrel doesn't sit along the
	 *  mesh +X the game aims, so the held gun reads as pointing off the crosshair; a per-Mk pitch
	 *  tilts it back onto the aim. ZERO/missing = no correction. Live-tunable via lr.HoldPitch/Yaw/
	 *  Roll (added on top) to find the value, then baked here. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "LaserRifle|PerTier")
	TArray<FRotator> LevelFineRot;

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
	float BeamDuration = 0.12f;   // longer so the BEAM reads as a beam (was 0.05 -> washed out by the muzzle bloom)
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

	// --- Per-Mk "shinies": spark/flame particles (low/mid tiers) + a plasma orb (high tiers). ---
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> SparkComps;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> SparkMIDs;
	TArray<FVector> SparkPos;
	TArray<FVector> SparkVel;
	TArray<float> SparkAge;
	TArray<float> SparkLife;
	TArray<float> SparkGrav;             // per-spark vertical accel (+down sparks / -up flames)
	TArray<float> SparkJitter;           // per-spark erratic sideways kick/sec (electrical zigzag; 0 = ballistic ember)
	TArray<FLinearColor> SparkColor;
	float SparkSpawnTimer = 0.f;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> PlasmaOrb = nullptr;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> PlasmaOrbMID = nullptr;
	float OrbPhase = 0.f;

	// --- Electrical ARC pool (procedural jagged lightning). Each arc owns SEGS_PER_ARC thin cylinder
	//     segments; the arc's jagged A->B poly-line is RE-DICED EVERY FRAME so the bolt flickers and
	//     writhes ("sparks"), not a static dash. TWO uses distinguished by ArcVel:
	//       * ArcVel == 0  -> ANCHORED JUMP: a bigger branching bolt fixed between two points
	//         (body-to-air jump when hot/firing, jump-to-surface near overheat, impact splash).
	//       * ArcVel != 0  -> FLOATING SPARK: a tiny jagged bolt that DRIFTS outward + falls each
	//         frame then fades -- the small "sparks that float away" ambient look driven by the slider.
	static constexpr int32 NUM_ARCS = 12;           // a few concurrent burst-sparks + jumps
	static constexpr int32 MAX_SEGS_PER_ARC = 40;   // floating spark = RADIAL BURST of ~12-16 filaments (dense);
	                                                // a gens<=3 anchored jump uses ~22 of these
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> ArcSegs;      // NUM_ARCS * MAX_SEGS_PER_ARC
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> ArcMIDs;
	TArray<FVector> ArcA, ArcB;        // per-arc world endpoints
	TArray<FVector> ArcVel;            // per-arc drift velocity (cm/s); 0 = anchored jump, !=0 = floating spark
	TArray<float> ArcAge, ArcLife, ArcChaos;
	TArray<int32> ArcGens;             // recursion depth (branch density): 2 = tiny spark, 3 = branching jump
	TArray<FLinearColor> ArcColor;
	float ArcSpawnTimer = 0.f;         // floating-spark spawn accumulator
	float JumpSpawnTimer = 0.f;        // big-jump spawn accumulator (only accrues when hot/firing)
	float GroundJumpTimer = 0.f;

	float FireCooldown = 0.f;
	bool bAttached = false;
	int32 LastDiagLevel = -1;   // per-tier geometry diag logs once per tier change
	int32 AppliedVisualLevel = -1;  // last Mk look applied; Tick re-applies live on research change
	float VisualPollTimer = 0.f;    // throttle for the live visual-level poll
	float Heat = 0.f;          // 0..1 heat; >=1 = overheated (locked until cooled)
	float LastShotAge = 99.f;  // seconds since the last shot; heat only COOLS when idle (not firing/reloading)
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

	// --- Energy: internal charge 'cell' (magazine = ShotsPerCell) refilled from 'portions'. ShotsPerCell scales
	//     with Mk (5->30 in ApplyVisualsForLevel). PORTIONS are now FUEL-DETERMINED (user 2026-07-12): each
	//     consumed fuel item grants its OWN portion count (see LR_FUEL_TABLE in the .cpp) -- NOT Mk-scaled.
	//     DoReload consumes the HIGHEST-portion fuel the player has. ---
	float ShotsPerCell = 30.f;          // a full cell = this many shots (Mk-scaled; 30 = Mk10)
	int32 PortionsPerBattery = 3;       // CURRENT pack max portions: set on each consume (= last fuel's portions)
	                                    // for the HUD bar; initial value = the free starter charge. NOT Mk-scaled now.
	float CellShots = -1.f;             // shots left in the cell (-1 = fill on first use)
	int32 Portions  = -1;               // recharge portions left in the current pack
	// Base-game Battery descriptor -- used ONLY by the parked dangling-battery VISUAL now (resolved once,
	// fail-open). The reload FUEL logic uses the fuel table below, not this.
	UPROPERTY() TSubclassOf<UFGItemDescriptor> BatteryItemClass = nullptr;
	bool  bBatteryResolveTried = false;
	// Fuel table (item class -> portions it grants). DoReload consumes the HIGHEST-portion fuel the player
	// carries (user 2026-07-12: "use the highest available fuel cell/battery as it provides more portions").
	// EXTENSIBLE: to add a new intermediate cell later, author the item + add ONE row to LR_FUEL_TABLE in the
	// .cpp -- no logic change. Resolved once, kept sorted portions-DESC; parallel arrays (class + portions).
	UPROPERTY() TArray<TSubclassOf<UFGItemDescriptor>> FuelClasses;
	TArray<int32> FuelPortions;         // parallel to FuelClasses
	bool  bFuelResolveTried = false;
	float RechargeFlash = 0.f;          // brief visual surge when a cell recharges
	float AmmoLogTimer = 0.f;           // throttle for the native-ammo diagnostic log
	bool  bNativeAmmoInit = false;      // one-time native ammo-class wiring done
	FTimerHandle BeamHandle;

	// PARKED 2026-07-05 (user: revisit later) -- feature OFF by default via lr.BatteryEnable (see the CVar in
	// the .cpp). Code kept intact; re-enable with lr.BatteryEnable 1.
	// --- Dangling Satisfactory battery (Mk1 frankenrifle): a real base-game Battery static mesh on a
	//     procedural pendulum, tied to the energy/reload state. It SWINGS while a cell is loaded,
	//     DISAPPEARS when the cell empties (reload failed) or during the swap's remove phase, and
	//     SLIDES BACK IN during the reload swap's up-phase ("goes in when reloading"). Reuses the
	//     DriveRigBones spring-damper swing -- physics/Verlet fails on the snap-to-aim hold (see the
	//     connector-history comment above). Mk1-only + lazily created; extend per-tier later. ---
	UPROPERTY() TObjectPtr<USceneComponent> BatteryPivot = nullptr;      // hang point on the body; this swings
	UPROPERTY() TObjectPtr<UStaticMeshComponent> BatteryCell = nullptr;  // the battery mesh, hangs below the pivot
	bool  bBatteryReady = false;          // GetItemMesh resolved + components created (Mk1, lazy; true even on fail so we stop retrying)
	float BatCellLen = 12.f;              // measured battery-mesh longest dim (cm); drives hang + slide distance
	FVector2D BatSwing = FVector2D::ZeroVector;      // pendulum angle (rad): X=fore/aft about pivot Y, Y=side about pivot X
	FVector2D BatSwingVel = FVector2D::ZeroVector;
	FVector BatPrevLoc = FVector::ZeroVector;
	FVector BatPrevVel = FVector::ZeroVector;
	bool  bBatVelInit = false;
	float BatInsert = 1.f;                // 0 = fully ejected/out, 1 = seated (drives the reload slide-in)
	float BatTime = 0.f;                  // idle-sway clock so a perfectly still gun still shows a little life
	UPROPERTY() TArray<TObjectPtr<UPoseableMeshComponent>> BatteryWires;   // power cables battery->gun (SK_WireChain)
	TArray<FBatWireSway> BatWireSway;     // per-cable spring-damper sway state (parallel to BatteryWires)

	// --- Rigged-body procedural bone driver (Mk1 pilot) ---
	UPROPERTY()
	TObjectPtr<USkeletalMesh> RiggedMesh = nullptr;   // SK_LaserRifle_Mk1c (constructor-loaded)
	UPROPERTY()
	TObjectPtr<UMaterialInterface> RiggedBodyMaterial = nullptr;   // M_Rifle_Mk01 -> textures the rigged Mk1
	bool bRiggedActive = false;        // rigged body currently shown (Mk1 + lr.RigEnable on)
	bool bRigSetup     = false;        // bone cache built in SetupRig
	bool bRigHasMountBones = false;    // body skeleton carries mount_1..mount_6 anchor bones
	                                   // (added 2026-07-03; wire body-ends anchor to these on
	                                   // the rig path instead of the hidden static body's frame)
	// Chain bones, in order: [0]=antenna_0 [1]=antenna_1 ; [2]=pend_0 [3]=pend_1 [4]=pend_2.
	FTransform RigRefCompRoot;         // component-space ref transform of the "root" bone
	TArray<FName>      RigBoneNames;   // the 5 driven bone names, chain order
	TArray<FTransform> RigRefLocal;    // ref LOCAL transform per driven bone (relative to parent)
	TArray<FVector2D>  RigAngle;       // per-joint swing angle (rad): X=about comp Y (fore/aft), Y=about comp X (side)
	TArray<FVector2D>  RigAngVel;      // per-joint angular velocity
	FVector RigPrevLoc = FVector::ZeroVector;   // reactive motion tracking
	FVector RigPrevVel = FVector::ZeroVector;
	bool    RigVelInit = false;
	// Per-instance randomized "feel" (seeded per spawned rifle so each jiggles differently).
	float RigIdleFreq = 1.3f, RigIdlePhase = 0.f, RigIdleAmp = 0.04f;
	float RigStiff = 90.f, RigDamp = 9.f, RigLag = 0.55f;
	float RigTime = 0.f;
	float RigNativeLongest = 110.f;    // rig mesh's longest dim (cm) measured at load; normalizes scale

	// --- Random kit-component loadout (lr.RandomComponents test command) ---
	// 6 pre-created, normally-relative-attached (NOT absolute -- they should just ride
	// BodyMesh's own world transform automatically, same as any ordinary child component).
	// SkeletalMeshComponents since the skeletal-pool pass: each part's own conn_N bones are
	// the connector anchors (no anim -- rigid ref pose).
	UPROPERTY()
	TArray<TObjectPtr<USkeletalMeshComponent>> LoadoutComps;
	int32 LastRandomComponentsValue = 0;   // edge-detect for the CVar (any change re-rolls)
	bool bHasRolledLoadout = false;        // false until the first roll; keeps comps hidden til then

	// Connector pool: up to 2 per loadout slot (12 total), each a UPoseableMeshComponent
	// showing SK_WireChain -- a thin skinned tube with a REAL 8-bone chain (wire_0..wire_7,
	// authored in Blender, imported by Scripts/ue/import_wirechain.py). UpdateConnectors()
	// poses the bones along the live sag curve every frame in component space -- the exact
	// mechanism DriveRigBones already uses for the rifle's own dangling battery. Per the
	// user's explicit direction: real skeletons, not spline/segment substitutes.
	UPROPERTY()
	TArray<TObjectPtr<UPoseableMeshComponent>> LoadoutConnectors;
	TArray<FKitConnectorState> ConnectorStates;
	/** SK_WireChain (constructor-loaded). NOT assigned to components until BeginPlay behind a
	 *  GetSkeleton() guard -- a skeleton that failed to cook would assert in
	 *  AllocateTransformData on register (see sf-skeletal-mesh-import-crash). */
	UPROPERTY()
	TObjectPtr<USkeletalMesh> WireChainMesh = nullptr;
	/** Wire surface: M_Kit_wire_sheathed (skeletal-usage flagged; see ctor comment). */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> WireChainMaterial = nullptr;
	bool bWireMeshReady = false;          // mesh assigned + bone cache built
	TArray<FName> WireBoneNames;          // wire_0..wire_7, chain order
	TArray<FQuat> WireRefRot;             // ref component-space rotation per bone
	TArray<FVector> WireRefScale;         // ref component-space scale per bone
	TArray<FVector> WireRefPos;           // ref component-space position per bone
	TArray<FVector> WireRefDir;           // ref chain direction at each bone (MEASURED from the
	                                      // imported skeleton -- never assume the FBX conversion
	                                      // kept the authoring axis; assuming +X smeared the skin
	                                      // off its anchors in-game: kinked rigid rods)
	TArray<int32> WireLocalAxis;          // which BONE-LOCAL axis the chain runs along, per bone
	float WireRefSegLen = 12.5f;          // ref spacing between chain bones (measured)

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
	/** Build the rig bone cache (indices, ref-local transforms, per-instance random feel). */
	void SetupRig();
	/** Pick 6 distinct random entries from KitComponentPool, place them at BodyMesh's 6
	 *  standard mount-fraction points (same formula as the Blender kit pipeline), scale each
	 *  self-correcting relative to BodyMesh's own longest dimension. Triggered by lr.RandomComponents
	 *  changing value (any change re-rolls; no-ops if KitComponentPool is empty). */
	void RollRandomComponents();
	/** Connector specs for a kit part: TYPE inferred from the asset name (battery/core/
	 *  capacitor -> power wire; coolant/fluid/hose -> pipe; grip/strap/hatch/spool -> direct
	 *  mount, none), ANCHORS read from the part skeleton's authored conn_0/conn_1 bones
	 *  (ref-pose component-space positions). A part with no conn bones gets no connectors. */
	TArray<FKitConnectorSpec> InferConnectorSpecs(const USkeletalMesh* Part) const;
	/** Pose every active connector's wire-bone chain from the two skeletons' CURRENT bone
	 *  transforms (part conn bone -> body mount bone), with spring-damped midpoint sway.
	 *  Called once per Tick, after whichever hold path (arms/camera) ran, so those
	 *  transforms are fresh. */
	void UpdateConnectors(float Dt);
	/** Pose the antenna + dangling-battery/wire bones each frame: reactive spring-pendulum on the
	 *  battery, floppy lag down the wire chain, idle sway on the antenna. RifleXf = the held world
	 *  transform applied to RiggedBody this frame (for component-frame motion). */
	void DriveRigBones(float Dt, const FTransform& RifleXf);
	/** Start the cell-swap motion + glow swell + fire lockout. Call from ANY recharge
	 *  source (auto-refill today, battery-inventory consumption later) — trigger-agnostic. */
	void OnCellRecharged();
	/** Consume a portion, refill the cell to full, and play the recharge beat. Shared by the empty-cell
	 *  auto-reload and the manual R-key reload. */
	void DoReload();
	/** Lazily resolve + cache the base-game Battery item descriptor (fail-open: returns null if not found).
	 *  Used ONLY by the parked dangling-battery VISUAL now; the reload fuel logic uses ResolveFuelTable(). */
	TSubclassOf<UFGItemDescriptor> ResolveBatteryClass();
	/** Lazily resolve + cache the fuel table (item class -> portions), sorted portions-DESC. Fail-open: a row
	 *  whose class won't load is skipped; an empty table -> DoReload free-refills so the gun can't brick. */
	void ResolveFuelTable();
	/** Count ALL spare fuel items (every accepted fuel class summed) for the HUD "spare" readout (-1 if none). */
	int32 CountSpareFuel();
	/** Lazily create the dangling-battery components (base-game Battery mesh via GetItemMesh) once BodyMesh exists (Mk1). */
	void SetupDanglingBattery();
	/** Pose + show/hide the dangling battery each frame from the energy/reload state (call after the hold path). */
	void UpdateDanglingBattery(float Dt);
	/** Pose one SK_WireChain poseable along a sagging, swaying curve between two WORLD points. Shared by the
	 *  battery cables; UpdateConnectors keeps its own inline copy for the kit wires (left untouched). */
	void PoseWireChainBetween(class UPoseableMeshComponent* Wire, const FVector& Start, const FVector& End,
		FVector& SwayOff, FVector& SwayVel, FVector& Sway2Off, FVector& Sway2Vel, FVector& PrevMid, bool& bSwayInit,
		float SimDt, bool bTaut);
	void UpdateHeatFX(float DeltaSeconds);
	void UpdateSmoke(float DeltaSeconds);
	void SpawnPuff(const FVector& Origin);
	/** Per-Mk spark/flame particles (low = orange sparks + overheat flames; mid = electric crackle). */
	void UpdateSparks(float DeltaSeconds);
	void SpawnSpark(const FLinearColor& Color, int32 Mode);   // Mode: 0=ember, 1=flame, 2=electric
	void UpdateArcs(float DeltaSeconds);
	int32 SpawnArc(const FVector& A, const FVector& B, float Life, float Chaos, const FLinearColor& Color, int32 Gens, const FVector& Vel = FVector::ZeroVector);
	/** Per-Mk plasma orb (high tiers): a glowing energy ball that bobs + pulses near the emitter. */
	void UpdatePlasmaOrb(float DeltaSeconds);
	float GripCfg(const TCHAR* StrId, float DefaultValue) const;
	bool  ConfigBool(const TCHAR* StrId, bool DefaultValue) const;
	void FireLaser(AFGCharacterPlayer* Char, class APlayerController* PC);
	void ShowBeam(const FVector& A, const FVector& B, const FLinearColor& Color);
	void HideBeam();
	void ForceCrosshair(class APlayerController* PC);
	void EnsureCrosshair(class APlayerController* PC, const FLinearColor& Color);
	FLinearColor LevelColor(int32 VisualLevel) const;
	/** The Mk this rifle should show/use: the item's FixedMkLevel if set, else the research level. */
	int32 EffectiveMkLevel() const;
	ALaserRifleSubsystem* GetSub() const;
	FLinearColor CurrentBeamColor() const;
};
