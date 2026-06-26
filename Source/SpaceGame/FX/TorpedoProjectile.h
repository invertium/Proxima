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

	/** Launch from Start toward Target, flying at Speed uu/s and dealing Damage on impact. */
	void Activate(const FVector& Start, AActor* InTarget, UMaterialInterface* Material,
		float InDamage, float InSpeed);

	/** Spawn + activate in one call. */
	static ATorpedoProjectile* Spawn(UWorld* World, const FVector& Start, AActor* Target,
		UMaterialInterface* Material, float Damage, float Speed);

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
	FVector LastTargetLoc = FVector::ZeroVector;
};
