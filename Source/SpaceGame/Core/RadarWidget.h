// SpaceGame — bridge simulator. Tactical radar (self-drawing UMG widget, DECISIONS D5).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RadarWidget.generated.h"

/**
 * URadarWidget — 2D tactical radar. Player is centred; orientation is north-up
 * (world +X = up, world +Y = right). Draws range rings, cross axes, a heading
 * arrow (rotates with the player ship's yaw), and a blip per URadarContactComponent
 * actor scaled into the radar by RadarRangeUU. All drawing is done in NativePaint
 * via UWidgetBlueprintLibrary line primitives — no textures, fully deterministic.
 * Embedded in the bridge HUD now; folded into the Helm/Tactical consoles at M6/M8.
 */
UCLASS()
class SPACEGAME_API URadarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	/** World distance (uu) from centre to the outer ring. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radar")
	float RadarRangeUU = 20000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radar")
	FLinearColor RingColor = FLinearColor(0.15f, 0.45f, 0.35f, 0.7f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radar")
	FLinearColor AxisColor = FLinearColor(0.12f, 0.3f, 0.28f, 0.6f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radar")
	FLinearColor PlayerColor = FLinearColor(0.2f, 0.9f, 1.0f, 1.0f);
};
