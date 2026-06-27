// SpaceGame — bridge simulator. Torpedo launcher (M17 Weapons second weapon type).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TorpedoLauncherComponent.generated.h"

class UWeaponComponent;
class UMaterialInterface;
class USoundBase;

/**
 * UTorpedoLauncherComponent — a limited-ammo torpedo tube that complements the beam.
 * It shares the sibling UWeaponComponent's locked target. Firing spends one round, spawns
 * a slow homing ATorpedoProjectile that flies to the target and detonates for a big burst
 * that bypasses shield mitigation, then enters a slow reload before the next launch.
 * The Weapons console drives + reads this alongside the beam.
 */
UCLASS(ClassGroup = (SpaceGame), meta = (BlueprintSpawnableComponent))
class SPACEGAME_API UTorpedoLauncherComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTorpedoLauncherComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Launch a torpedo at the shared target if armed (ammo left, reloaded, target locked). */
	UFUNCTION(BlueprintCallable, Category = "Ship|Torpedo")
	bool Fire();

	UFUNCTION(BlueprintPure, Category = "Ship|Torpedo")
	int32 GetAmmo() const { return Ammo; }

	UFUNCTION(BlueprintPure, Category = "Ship|Torpedo")
	int32 GetMaxAmmo() const { return MaxAmmo; }

	/** Ready to fire: a round chambered, reload finished, and a target locked. */
	UFUNCTION(BlueprintPure, Category = "Ship|Torpedo")
	bool IsReady() const;

	/** Reload progress 0..1 (1 = chambered/ready). */
	UFUNCTION(BlueprintPure, Category = "Ship|Torpedo")
	float GetReloadFraction() const;

	/** Refill the magazine to MaxAmmo and clear the reload timer (station resupply, M19). */
	UFUNCTION(BlueprintCallable, Category = "Ship|Torpedo")
	void Resupply() { Ammo = MaxAmmo; ReloadTimer = 0.f; }

	/** Rounds carried at spawn (and the magazine cap). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Torpedo")
	int32 MaxAmmo = 4;

	/** Seconds between launches. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Torpedo")
	float ReloadTime = 4.f;

	/** Full forward launch-arc width in degrees. Wider than the beam's — the torpedo homes after
	 *  launch, so the tube only needs the target roughly ahead, not pinpoint on the bow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Torpedo")
	float FireArcDeg = 110.f;

	/** Hull damage delivered on impact (bypasses shield mitigation). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Torpedo")
	float TorpedoDamage = 60.f;

	/** Projectile travel speed (uu/s) — slow enough to watch it close in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Torpedo")
	float TorpedoSpeed = 5000.f;

	/** Camera-shake trauma added to the firing ship on each launch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Torpedo")
	float FireTrauma = 0.35f;

protected:
	virtual void BeginPlay() override;

	// --- Authoritative runtime state (replication-ready, D7) ---
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ship|Torpedo")
	int32 Ammo = 4;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ship|Torpedo")
	float ReloadTimer = 0.f; // seconds left until the next round is chambered

private:
	/** Shared target comes from the sibling beam weapon (lazily resolved). */
	UPROPERTY(Transient)
	TObjectPtr<UWeaponComponent> CachedWeapon;

	UWeaponComponent* GetWeapon() const;

	/** Emissive material for the torpedo + impact FX (orange), loaded in the constructor. */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> TorpedoMaterial;

	/** Launch SFX (reuses the beam-fire cue), loaded in the constructor. */
	UPROPERTY(EditAnywhere, Category = "Ship|Torpedo")
	TObjectPtr<USoundBase> FireSound;
};
