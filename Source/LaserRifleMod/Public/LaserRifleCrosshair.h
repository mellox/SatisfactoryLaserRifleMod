#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LaserRifleCrosshair.generated.h"

/** Minimal full-screen crosshair drawn in C++ (4 ticks + center dot). No asset
 *  needed; colour tracks the current beam colour. */
UCLASS()
class LASERRIFLEMOD_API ULaserRifleCrosshair : public UUserWidget
{
	GENERATED_BODY()
public:
	FLinearColor CrosshairColor = FLinearColor(0.2f, 1.f, 0.4f);

	// Energy readout (set by the weapon each tick): cell shots + battery portions.
	int32 CellShots = 0;
	int32 CellMax = 30;
	int32 Portions = 0;
	int32 PortionsMax = 8;
	bool  bOverheated = false;

	// Native ammo HUD does NOT render for this weapon (it bypasses the vanilla fire
	// pipeline), confirmed in-game 2026-06-24 → custom readout restored. Native ammo
	// fields are still driven (data-only) for a future battery-as-inventory-item path.
	bool  bDrawCustomEnergy = true;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
};
