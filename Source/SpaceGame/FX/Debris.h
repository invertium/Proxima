// SpaceGame — bridge simulator. Drifting hull-debris chunk (M22).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Debris.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;

/**
 * ADebris — a small jagged chunk flung from a destroyed ship. It drifts along an initial velocity
 * (with a little drag), tumbles, and shrinks away at the end of its life before self-destructing.
 * Pure transform animation, no collision — purely cosmetic wreckage.
 */
UCLASS()
class SPACEGAME_API ADebris : public AActor
{
	GENERATED_BODY()

public:
	ADebris();
	virtual void Tick(float DeltaSeconds) override;

	/** Place a chunk, give it a drift velocity + tint + rough size, and run for Life seconds. */
	void Activate(const FVector& Location, const FVector& Velocity, UMaterialInterface* Material,
		float Scale, float Life);

	/** Spawn + activate in one call. */
	static ADebris* Spawn(UWorld* World, const FVector& Location, const FVector& Velocity,
		UMaterialInterface* Material, float Scale, float Life);

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;

private:
	FVector Velocity = FVector::ZeroVector;
	FRotator Spin = FRotator::ZeroRotator;
	FVector PeakScale = FVector::OneVector;
	float Remaining = 0.f;
	float TotalLife = 0.f;
};
