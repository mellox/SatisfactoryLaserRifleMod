#include "LaserRifleCrosshair.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "GameFramework/PlayerController.h"

int32 ULaserRifleCrosshair::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
		OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	// Draw nothing while a menu shows the cursor (inventory/escape). Done here (not via
	// SetVisibility) so the widget keeps painting and reappears when the menu closes.
	const APlayerController* PC = GetOwningPlayer();
	if (PC && PC->bShowMouseCursor) { return Layer; }

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const FVector2D C    = Size * 0.5f;
	const float Gap = 6.f, Len = 14.f, Thick = 2.f;
	const FPaintGeometry PG = AllottedGeometry.ToPaintGeometry();

	auto Line = [&](const FVector2D& A, const FVector2D& B)
	{
		TArray<FVector2D> Pts; Pts.Add(A); Pts.Add(B);
		FSlateDrawElement::MakeLines(OutDrawElements, Layer + 1, PG, Pts,
			ESlateDrawEffect::None, CrosshairColor, true, Thick);
	};
	Line(C + FVector2D(-Gap - Len, 0.f), C + FVector2D(-Gap, 0.f));
	Line(C + FVector2D( Gap, 0.f),       C + FVector2D( Gap + Len, 0.f));
	Line(C + FVector2D(0.f, -Gap - Len), C + FVector2D(0.f, -Gap));
	Line(C + FVector2D(0.f,  Gap),       C + FVector2D(0.f,  Gap + Len));
	// center dot
	TArray<FVector2D> Dot; Dot.Add(C + FVector2D(-1.f, 0.f)); Dot.Add(C + FVector2D(1.f, 0.f));
	FSlateDrawElement::MakeLines(OutDrawElements, Layer + 1, PG, Dot,
		ESlateDrawEffect::None, CrosshairColor, true, 3.f);

	// --- Custom energy readout: only when the native ammo HUD fallback is enabled.
	//     The native AFGWeapon ammo count is the primary display now. ---
	if (!bDrawCustomEnergy) { return Layer + 1; }
	const FLinearColor EnergyCol = bOverheated ? FLinearColor(1.f, 0.25f, 0.15f) : CrosshairColor;
	const float Frac = (CellMax > 0) ? FMath::Clamp((float)CellShots / (float)CellMax, 0.f, 1.f) : 0.f;
	const float BarW = 150.f;
	const FVector2D BarL(95.f, Size.Y - 215.f);   // bottom-left, lifted clear of the hotbar/inventory row
	auto Bar = [&](const FVector2D& A, const FVector2D& B, const FLinearColor& Col, float T)
	{
		TArray<FVector2D> P; P.Add(A); P.Add(B);
		FSlateDrawElement::MakeLines(OutDrawElements, Layer + 2, PG, P,
			ESlateDrawEffect::None, Col, true, T);
	};
	Bar(BarL, BarL + FVector2D(BarW, 0.f), FLinearColor(0.f, 0.f, 0.f, 0.5f), 11.f);        // track
	Bar(BarL, BarL + FVector2D(BarW * Frac, 0.f), EnergyCol, 11.f);                          // fill
	FString Txt = FString::Printf(TEXT("%d / %d    BATT %d/%d"),
		CellShots, CellMax, Portions, PortionsMax);
	if (SpareBatteries >= 0) { Txt += FString::Printf(TEXT("   x%d spare"), SpareBatteries); }   // inventory Batteries
	FSlateDrawElement::MakeText(OutDrawElements, Layer + 3,
		AllottedGeometry.ToOffsetPaintGeometry(BarL + FVector2D(0.f, 10.f)),
		Txt, FCoreStyle::GetDefaultFontStyle("Bold", 15), ESlateDrawEffect::None, EnergyCol);

	return Layer + 3;
}
