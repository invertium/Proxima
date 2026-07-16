// SpaceGame — bridge simulator. Limited-ammo torpedo projectile (M17 Weapons).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TorpedoProjectile.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;

/**
 * ATorpedoProjectile — a slow, glowing round that homes on a target actor and, on arrival,
 * deals a big burst of damage that BYPASSES shield mitigation (straight to hull). Unlike the
 * instant beam, it travels visibly across the battlefield, so the crew watches it close in.
 * Self-destructs on impact, if the target is lost, or after MaxLifetime.
 */
UCLASS()
class SPACEGAME_API ATorpedoProjectile : public AActor
{
	GENERATED_BODY()

public:
	ATorpedoProjectile();
	virtual void Tick(float DeltaSeconds) override;

	/** Launch from Start toward Target, flying at Speed uu/s and dealing Damage on impact.
	 *  Lifetime caps the chase (M26: enemy volleys fly longer than the player's snap shots). */
	void Activate(const FVector& Start, AActor* InTarget, UMaterialInterface* Material,
		float InDamage, float InSpeed, float InLifetime = 6.f);

	/** Spawn + activate in one call. */
	static ATorpedoProjectile* Spawn(UWorld* World, const FVector& Start, AActor* Target,
		UMaterialInterface* Material, float Damage, float Speed, float Lifetime = 6.f);

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;

private:
	/** Land the payload (hull-only) on the target and spawn an impact flash, then destroy. */
	void Detonate(const FVector& At);

	TWeakObjectPtr<AActor> Target;
	UPROPERTY() TObjectPtr<UMaterialInterface> FxMaterial;
	float Damage = 60.f;
	float Speed = 6000.f;
	float Age = 0.f;
	float MaxLifetime = 6.f;     // safety despawn if it never reaches the target
	float HitRadius = 350.f;     // detonation proximity (uu)
	float BlastRadius = 700.f;   // payload only lands if the target is inside this (M26: evadable)
	FVector LastTargetLoc = FVector::ZeroVector;

	/** Current travel direction (unit). The torpedo steers this toward the target at TurnRateDeg
	 *  per second rather than snapping straight at it — so a hard turn or a warp jump slips it
	 *  (issue #2: no instant 180s, no guaranteed hit). Set to the launch bearing on Activate. */
	FVector Heading = FVector::ForwardVector;

	/** Max steering rate (deg/s). ~75 lets it gently track a maneuvering ship but never reverse
	 *  fast enough to re-acquire a target that jumps or juke away. */
	float TurnRateDeg = 75.f;
};
