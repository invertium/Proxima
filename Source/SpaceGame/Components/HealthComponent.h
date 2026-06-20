// SpaceGame — bridge simulator. Hull/shield health (sim core, DECISIONS D5/D7).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHealthDeath, AActor*, DeadActor);

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
	 */
	UFUNCTION(BlueprintCallable, Category = "Ship|Health")
	float ApplyDamage(float Amount);

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

	/** Broadcast once when hull first reaches zero. */
	UPROPERTY(BlueprintAssignable, Category = "Ship|Health")
	FOnHealthDeath OnDeath;

	/** Starting + maximum hull hit-points. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Health")
	float MaxHull = 100.f;

	/** Starting + maximum shield pool (absorbed before hull). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Health")
	float MaxShield = 50.f;

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
};
