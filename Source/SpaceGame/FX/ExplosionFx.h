// SpaceGame — bridge simulator. Transient emissive explosion flash (replaces debug spheres).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ExplosionFx.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class USoundBase;

/**
 * AExplosionFx — a glowing sphere that expands then collapses over a short life and
 * self-destructs (M13 FX). Spawned where a ship dies. Pure geometry animation (no
 * dynamic material needed): scale follows sin(t·π) so it flashes out and fades back.
 */
UCLASS()
class SPACEGAME_API AExplosionFx : public AActor
{
	GENERATED_BODY()

public:
	AExplosionFx();
	virtual void Tick(float DeltaSeconds) override;

	/** Centre the flash, set its peak radius (uu), tint it, and run for Life seconds.
	 *  bPlaySound gates the boom SFX (off for small beam-impact flashes). */
	void Activate(const FVector& Location, float PeakRadius, UMaterialInterface* Material, float Life,
		bool bPlaySound = true);

	/** Spawn + activate in one call. */
	static AExplosionFx* Spawn(UWorld* World, const FVector& Location, float PeakRadius,
		UMaterialInterface* Material, float Life = 0.6f, bool bPlaySound = true);

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;

	/** Explosion SFX (CC0), loaded in the constructor; played at the blast location. */
	UPROPERTY() TObjectPtr<USoundBase> ExplosionSound;

private:
	float Remaining = 0.f;
	float TotalLife = 0.f;
	float PeakScale = 1.f;
};
