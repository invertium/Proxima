// SpaceGame — bridge simulator. Beam weapon (sim core, DECISIONS D5/D11).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponComponent.generated.h"

class UPowerComponent;
class UMaterialInterface;
class USoundBase;

/**
 * UWeaponComponent — a single forward beam weapon. Charges 0..1 over time at a rate
 * scaled by the ship's Weapons power (DECISIONS D11): no weapon power → never charges.
 * Firing requires a full charge and an in-range target; it logs the beam event, draws
 * a debug beam, and empties the charge. Targeting cycles through radar-contact actors
 * (URadarContactComponent), excluding the player. Damage lands once enemies gain health
 * (M9); for now the shot is logged + drawn. The Weapons console drives + reads this.
 */
UCLASS(ClassGroup = (SpaceGame), meta = (BlueprintSpawnableComponent))
class SPACEGAME_API UWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Cycle the current target to the next radar contact (wraps; clears if none). */
	UFUNCTION(BlueprintCallable, Category = "Ship|Weapon")
	void CycleTarget();

	/** Fire the beam if fully charged and a target is in range. Returns true if it fired. */
	UFUNCTION(BlueprintCallable, Category = "Ship|Weapon")
	bool FireBeam();

	UFUNCTION(BlueprintPure, Category = "Ship|Weapon")
	float GetCharge() const { return Charge; }

	UFUNCTION(BlueprintPure, Category = "Ship|Weapon")
	AActor* GetCurrentTarget() const { return CurrentTarget; }

	/** Distance to the current target in uu, or -1 if none. */
	UFUNCTION(BlueprintPure, Category = "Ship|Weapon")
	float GetTargetRange() const;

	/** True while the current target is within BeamRange. */
	UFUNCTION(BlueprintPure, Category = "Ship|Weapon")
	bool IsTargetInRange() const;

	/** Beam reach (uu). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Weapon")
	float BeamRange = 15000.f;

	/** Charge gained per second at nominal (1.0×) weapon power. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Weapon")
	float BaseRechargeRate = 0.4f;

	/** Damage dealt to the target's health on a hit (shields absorb first, then hull). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Weapon")
	float BeamDamage = 25.f;

	/** Seconds the fired beam FX stays visible. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Weapon")
	float BeamDrawTime = 0.2f;

	/** Camera-shake trauma added to the firing ship on each shot (recoil kick, M14). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Weapon")
	float FireTrauma = 0.28f;

protected:
	// --- Authoritative runtime state (replication-ready, D7) ---
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ship|Weapon")
	float Charge = 0.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ship|Weapon")
	TObjectPtr<AActor> CurrentTarget;

private:
	/** Weapons power multiplier from the sibling power component, or 1.0 if none. */
	float WeaponPowerScale() const;

	/** Lazily-resolved sibling power component; may stay null. */
	UPROPERTY(Transient)
	TObjectPtr<UPowerComponent> CachedPower;

	/** Emissive material for the beam FX (cyan), loaded in the constructor. */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> BeamMaterial;

	/** Beam-fire SFX (CC0), loaded in the constructor; played 2D on each shot. */
	UPROPERTY(EditAnywhere, Category = "Ship|Weapon")
	TObjectPtr<USoundBase> FireSound;
};
