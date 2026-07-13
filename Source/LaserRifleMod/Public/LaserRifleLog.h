#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "LaserRifleMod.h"   // LogLaserRifle category

/**
 * Per-category gating for the mod's [LR] Display-level diagnostics.
 *
 * PROBLEM: the mod's UE_LOG(LogLaserRifle, Display, ...) diagnostics are USEFUL for debugging
 * (fuel, beam, rig, FX, visuals, native ammo) but SPAM the Shipping FactoryGame.log when left on
 * for a public release. This header makes them QUIET BY DEFAULT (every CVar below defaults 0)
 * while still letting a dev re-enable exactly the category they're debugging:
 *   lr.Debug 1      -- show every [LR] Display diagnostic at once (master switch)
 *   lr.LogFuel 1    -- fuel/reload/cell/portion/Energy Cell/battery/spare
 *   lr.LogBeam 1    -- beam trace/BEAMDIAG/muzzle/crosshair
 *   lr.LogFx 1      -- sparks/arcs/plasma/smoke/heat/overheat/pulse FX
 *   lr.LogRig 1     -- rig/bones/pendulum/wire/dangling/antenna/kit/connector/loadout
 *   lr.LogVisual 1  -- per-Mk visual/mesh/geometry/tier setup
 *   lr.LogAmmo 1    -- native ammo HUD sync (NativeAmmo init/sync)
 *   lr.LogGeneral 1 -- anything that didn't clearly fit another bucket
 *
 * UE_LOG(..., Warning, ...) and UE_LOG(..., Error, ...) lines are NEVER routed through this --
 * they must always show (fail-open/fail-loud diagnostics), so leave those as plain UE_LOG calls.
 */

extern TAutoConsoleVariable<int32> CVarLrDebug;
extern TAutoConsoleVariable<int32> CVarLrLogFuel;
extern TAutoConsoleVariable<int32> CVarLrLogBeam;
extern TAutoConsoleVariable<int32> CVarLrLogFx;
extern TAutoConsoleVariable<int32> CVarLrLogRig;
extern TAutoConsoleVariable<int32> CVarLrLogVisual;
extern TAutoConsoleVariable<int32> CVarLrLogAmmo;
extern TAutoConsoleVariable<int32> CVarLrLogGeneral;

// Gate a single Display-level [LR] diagnostic behind the master switch OR its own category
// switch. CatVar is one of the CVarLrLog* globals above (NOT a string). Only ever wrap a
// Display diagnostic in this -- Warning/Error UE_LOG calls must stay ungated.
#define LR_LOG(CatVar, ...) \
	do { if (CVarLrDebug.GetValueOnGameThread() != 0 || (CatVar).GetValueOnGameThread() != 0) \
	{ UE_LOG(LogLaserRifle, Display, __VA_ARGS__); } } while (0)
