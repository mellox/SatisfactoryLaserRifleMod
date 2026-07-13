#include "LaserRifleLog.h"

// All default OFF (0): [LR] Display-level diagnostics are silent unless a dev opts in, so a
// public/Shipping build stays quiet. See LaserRifleLog.h for the full category list + LR_LOG.
TAutoConsoleVariable<int32> CVarLrDebug(TEXT("lr.Debug"), 0,
	TEXT("LaserRifle: 1 = show every [LR] Display-level diagnostic, across all categories (master switch)."));

TAutoConsoleVariable<int32> CVarLrLogFuel(TEXT("lr.LogFuel"), 0,
	TEXT("LaserRifle: 1 = show [LR] fuel/reload/cell/portion/Energy Cell/battery diagnostics."));

TAutoConsoleVariable<int32> CVarLrLogBeam(TEXT("lr.LogBeam"), 0,
	TEXT("LaserRifle: 1 = show [LR] beam trace/BEAMDIAG/muzzle/crosshair diagnostics."));

TAutoConsoleVariable<int32> CVarLrLogFx(TEXT("lr.LogFx"), 0,
	TEXT("LaserRifle: 1 = show [LR] spark/arc/plasma/smoke/heat/overheat/pulse FX diagnostics."));

TAutoConsoleVariable<int32> CVarLrLogRig(TEXT("lr.LogRig"), 0,
	TEXT("LaserRifle: 1 = show [LR] rig/bone/pendulum/wire/dangling/antenna/kit/connector/loadout diagnostics."));

TAutoConsoleVariable<int32> CVarLrLogVisual(TEXT("lr.LogVisual"), 0,
	TEXT("LaserRifle: 1 = show [LR] per-Mk visual/mesh/geometry/tier diagnostics."));

TAutoConsoleVariable<int32> CVarLrLogAmmo(TEXT("lr.LogAmmo"), 0,
	TEXT("LaserRifle: 1 = show [LR] native ammo HUD sync diagnostics."));

TAutoConsoleVariable<int32> CVarLrLogGeneral(TEXT("lr.LogGeneral"), 0,
	TEXT("LaserRifle: 1 = show [LR] diagnostics that don't fit another category."));
