// SpaceGame — bridge simulator. Tactical radar implementation.

#include "Core/RadarWidget.h"

#include "Components/DamageControlComponent.h"
#include "Components/RadarContactComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "GameFramework/Pawn.h"
#include "UObject/UObjectIterator.h"

namespace
{
	// Connected polyline for a circle of NumSegments around Center.
	void DrawCircle(FPaintContext& Ctx, const FVector2D& Center, float Radius,
		const FLinearColor& Color, float Thickness = 1.0f, int32 NumSegments = 48)
	{
		TArray<FVector2D> Pts;
		Pts.Reserve(NumSegments + 1);
		for (int32 i = 0; i <= NumSegments; ++i)
		{
			const float A = (2.f * PI * i) / NumSegments;
			Pts.Add(Center + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Radius);
		}
		UWidgetBlueprintLibrary::DrawLines(Ctx, Pts, Color, true, Thickness);
	}

	void DrawSeg(FPaintContext& Ctx, const FVector2D& A, const FVector2D& B,
		const FLinearColor& Color, float Thickness = 1.0f)
	{
		TArray<FVector2D> Pts = { A, B };
		UWidgetBlueprintLibrary::DrawLines(Ctx, Pts, Color, true, Thickness);
	}
}

int32 URadarWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
		OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	FPaintContext Ctx(AllottedGeometry, MyCullingRect, OutDrawElements, BaseLayer + 1,
		InWidgetStyle, bParentEnabled);

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const FVector2D Center = Size * 0.5;
	const float R = FMath::Min(Size.X, Size.Y) * 0.5f - 6.f;
	if (R <= 0.f)
	{
		return BaseLayer;
	}

	// Range rings + cross axes.
	DrawCircle(Ctx, Center, R, RingColor, 1.5f);
	DrawCircle(Ctx, Center, R * 0.66f, RingColor);
	DrawCircle(Ctx, Center, R * 0.33f, RingColor);
	DrawSeg(Ctx, Center - FVector2D(R, 0), Center + FVector2D(R, 0), AxisColor);
	DrawSeg(Ctx, Center - FVector2D(0, R), Center + FVector2D(0, R), AxisColor);

	// Player transform.
	const APawn* Player = GetOwningPlayerPawn();
	const UWorld* World = GetWorld();
	if (Player && World)
	{
		const FVector PlayerLoc = Player->GetActorLocation();
		const float YawRad = FMath::DegreesToRadians(Player->GetActorRotation().Yaw);

		// Damaged sensors halve the radar's reach (M25) — distant contacts pin to the edge ring.
		float SensorMult = 1.f;
		if (const UDamageControlComponent* Dmg = Player->FindComponentByClass<UDamageControlComponent>())
		{
			SensorMult = Dmg->GetMultiplier(EDamageSystem::Sensors);
		}
		const float Scale = R / FMath::Max(RadarRangeUU * SensorMult, 1.f);

		// world +X = up (-screenY), world +Y = right (+screenX)
		auto WorldToScreen = [&](const FVector& Delta) -> FVector2D
		{
			FVector2D S(Delta.Y * Scale, -Delta.X * Scale);
			if (S.Size() > R) { S = S.GetSafeNormal() * R; } // clamp to edge
			return S;
		};

		// Blips for every radar contact (skip the player's own ship).
		for (TObjectIterator<URadarContactComponent> It; It; ++It)
		{
			const URadarContactComponent* C = *It;
			if (!IsValid(C) || C->GetWorld() != World) { continue; }
			const AActor* A = C->GetOwner();
			if (!A || A == Player) { continue; }

			const FVector2D P = Center + WorldToScreen(A->GetActorLocation() - PlayerLoc);
			DrawCircle(Ctx, P, 5.f, C->BlipColor, 1.5f, 12);
			DrawSeg(Ctx, P - FVector2D(7, 0), P + FVector2D(7, 0), C->BlipColor);
			DrawSeg(Ctx, P - FVector2D(0, 7), P + FVector2D(0, 7), C->BlipColor);
		}

		// Player marker + heading arrow (forward = world +X, rotated by yaw).
		const FVector Fwd(FMath::Cos(YawRad), FMath::Sin(YawRad), 0.f);
		const FVector2D FwdScreen = FVector2D(Fwd.Y, -Fwd.X); // unit, screen space
		const FVector2D Tip = Center + FwdScreen * 22.f;
		const FVector2D Perp(-FwdScreen.Y, FwdScreen.X);
		DrawCircle(Ctx, Center, 4.f, PlayerColor, 1.5f, 12);
		DrawSeg(Ctx, Center, Tip, PlayerColor, 2.f);
		DrawSeg(Ctx, Tip, Tip - FwdScreen * 8.f + Perp * 5.f, PlayerColor, 2.f);
		DrawSeg(Ctx, Tip, Tip - FwdScreen * 8.f - Perp * 5.f, PlayerColor, 2.f);
	}

	return FMath::Max(BaseLayer, Ctx.MaxLayer);
}
