// SpaceGame — bridge simulator. Ship impulse movement (simulation core, DECISIONS D5/D11).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShipMovementComponent.generated.h"

/**
 * UShipMovementComponent — authoritative impulse movement for a ship.
 * Throttle (0..1) eases CurrentSpeed toward a target along the owner's +X;
 * Turn (-1..1) yaws the owner. Inputs are set externally (M3 test harness now,
 * Helm console at M6; Engineering power scales MaxSpeed/turn at M7).
 * State is plain UPROPERTYs so it stays replication-ready (DECISIONS D7).
 */
UCLASS(ClassGroup = (SpaceGame), meta = (BlueprintSpawnableComponent))
class SPACEGAME_API UShipMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UShipMovementComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Set impulse throttle, 0..1 (fraction of MaxSpeed). */
	UFUNCTION(BlueprintCallable, Category = "Ship|Movement")
	void SetThrottle(float InThrottle);

	/** Set yaw turn, -1..1 (left .. right). */
	UFUNCTION(BlueprintCallable, Category = "Ship|Movement")
	void SetTurn(float InTurn);

	/** Lock out helm input and freeze the ship (used while docked, M19). Locking zeroes the
	 *  current throttle/turn so the ship comes to rest; SetThrottle/SetTurn are ignored until
	 *  unlocked. */
	UFUNCTION(BlueprintCallable, Category = "Ship|Movement")
	void SetInputLocked(bool bLocked);

	UFUNCTION(BlueprintPure, Category = "Ship|Movement")
	bool IsInputLocked() const { return bInputLocked; }

	UFUNCTION(BlueprintPure, Category = "Ship|Movement")
	float GetSpeed() const { return CurrentSpeed; }

	UFUNCTION(BlueprintPure, Category = "Ship|Movement")
	float GetThrottle() const { return ThrottleInput; }

	/** Max speed after Engineering power scaling (MaxSpeed * engine power; MaxSpeed if no power comp). */
	UFUNCTION(BlueprintPure, Category = "Ship|Movement")
	float GetEffectiveMaxSpeed() const;

	// --- Tunables (Engineering engine power scales MaxSpeed, M7) ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Movement")
	float MaxSpeed = 1800.f; // uu/s at full throttle

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Movement")
	float Acceleration = 1200.f; // uu/s^2, constant approach to target speed

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Movement")
	float MaxTurnRate = 60.f; // deg/s at full turn

protected:
	// --- Authoritative runtime state (replication-ready) ---
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ship|Movement")
	float ThrottleInput = 0.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ship|Movement")
	float TurnInput = 0.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ship|Movement")
	float CurrentSpeed = 0.f;

	/** While true, throttle/turn inputs are ignored and held at zero (docked, M19). */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ship|Movement")
	bool bInputLocked = false;

private:
	/** Lazily-resolved sibling power component (engine power scales MaxSpeed); may stay null. */
	UPROPERTY(Transient)
	TObjectPtr<class UPowerComponent> CachedPower;

	/** Engine power multiplier from the power component, or 1.0 if there is none. */
	float EnginePowerScale() const;
};
