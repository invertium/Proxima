// SpaceGame — bridge simulator. Transient emissive beam effect (replaces debug lines).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BeamFx.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;

/**
 * ABeamFx — a short-lived glowing cylinder stretched between two points, used for
 * weapon beams (M13 FX). Spawn it, call Activate(start, end, …), and it orients +
 * scales itself, fades its width over its lifetime, and self-destructs. Cheap and
 * reusable by the player weapon (cyan) and enemy fire (orange).
 */
UCLASS()
class SPACEGAME_API ABeamFx : public AActor
{
	GENERATED_BODY()

public:
	ABeamFx();
	virtual void Tick(float DeltaSeconds) override;

	/** Point the beam from Start to End, tint it, and start its fade-out over Life seconds. */
	void Activate(const FVector& Start, const FVector& End, UMaterialInterface* Material, float Radius, float Life);

	/** Spawn + activate in one call. */
	static ABeamFx* Spawn(UWorld* World, const FVector& Start, const FVector& End,
		UMaterialInterface* Material, float Radius = 22.f, float Life = 0.18f);

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;

private:
	float Remaining = 0.f;
	float TotalLife = 0.f;
	float BaseRadius = 22.f;
	FVector Mid = FVector::ZeroVector;
	FRotator Orient = FRotator::ZeroRotator;
	float LengthScale = 1.f;
};
