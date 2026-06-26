// SpaceGame — bridge simulator. Hull/shield health (sim core, DECISIONS D5/D7).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

class UPowerComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHealthDeath, AActor*, DeadActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthDamaged, float, EffectiveDamage, float, HullRemaining);

/**
 * UHealthComponent — hull + regenerating-free shield pool for any combatant. Damage
 * lands shields-first, then hull (DECISIONS D11 — shield power will scale mitigation at
 * M11). ApplyDamage() is the single authoritative entry point: it spends the amount,
 * logs the result, and fires OnDeath once hull reaches zero (despawn/explosion is M10).
 * Enemies (AEnemyShip) carry one; the player ship gains one at M11. Replication-ready
 * plain UPROPERTYs (D7).
 */
UCLASS(ClassGroup = (SpaceGame), meta = (BlueprintSpawnableComponent))
class SPACEGAME_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();

	/**
	 * Apply incoming damage: shields absorb first, the remainder hits hull. Returns the
	 * hull remaining. Fires OnDeath the first time hull crosses to zero.
	 * bBypassShield (torpedoes, M17) skips both shield-power mitigation and the shield pool,
	 * landing the full amount straight on hull.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ship|Health")
	float ApplyDamage(float Amount, bool bBypassShield = false);

	/** Restore hull (never above MaxHull; a dead hull stays dead). Returns hull remaining. */
	UFUNCTION(BlueprintCallable, Category = "Ship|Health")
	float RepairHull(float Amount);

	/** Re-fill hull + shield pools to their current Max and clear the death latch. Used when
	 *  a ship variant/type sets new Max values at runtime (M18 enemy types / player ships). */
	UFUNCTION(BlueprintCallable, Category = "Ship|Health")
	void ResetPools();

	UFUNCTION(BlueprintPure, Category = "Ship|Health")
	float GetHull() const { return Hull; }

	UFUNCTION(BlueprintPure, Category = "Ship|Health")
	float GetMaxHull() const { return MaxHull; }

	UFUNCTION(BlueprintPure, Category = "Ship|Health")
	float GetShield() const { return Shield; }

	UFUNCTION(BlueprintPure, Category = "Ship|Health")
	float GetMaxShield() const { return MaxShield; }

	UFUNCTION(BlueprintPure, Category = "Ship|Health")
	bool IsAlive() const { return Hull > 0.f; }

	/** Current incoming-damage mitigation (0..MaxMitigation) from sibling shield power. */
	UFUNCTION(BlueprintPure, Category = "Ship|Health")
	float GetShieldMitigation() const;

	/** Broadcast once when hull first reaches zero. */
	UPROPERTY(BlueprintAssignable, Category = "Ship|Health")
	FOnHealthDeath OnDeath;

	/** Broadcast on every damaging hit (effective damage dealt + hull remaining), for FX/feedback. */
	UPROPERTY(BlueprintAssignable, Category = "Ship|Health")
	FOnHealthDamaged OnDamaged;

	/** Starting + maximum hull hit-points. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Health")
	float MaxHull = 100.f;

	/** Starting + maximum shield pool (absorbed before hull). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Health")
	float MaxShield = 50.f;

	/** Mitigation gained per unit of sibling Shields power (D11). 1.0 power → this much. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Health")
	float ShieldMitigationScale = 0.35f;

	/** Hard cap on shield-power mitigation, so damage never reaches zero. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Health")
	float MaxMitigation = 0.8f;

protected:
	virtual void BeginPlay() override;

	// --- Authoritative runtime state (replication-ready, D7) ---
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ship|Health")
	float Hull = 100.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ship|Health")
	float Shield = 50.f;

private:
	/** Guards OnDeath against re-broadcast on overkill / repeated hits. */
	bool bDeathBroadcast = false;

	/** Lazily-resolved sibling power component (for shield-power mitigation); may stay null. */
	UPROPERTY(Transient)
	mutable TObjectPtr<UPowerComponent> CachedPower;
};
