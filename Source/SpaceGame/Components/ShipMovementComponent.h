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

	/** Set lateral strafe thrust, -1..1 (port .. starboard) — evasive side-slip (issue #7). Slides
	 *  the ship sideways without turning the bow, so you can dodge fire and keep guns on target. */
	UFUNCTION(BlueprintCallable, Category = "Ship|Movement")
	void SetStrafe(float InStrafe);

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

	/** Lower bound for throttle input (negative = reverse allowed). */
	UFUNCTION(BlueprintPure, Category = "Ship|Movement")
	float GetReverseThrottleMin() const { return ReverseThrottleMin; }

	/** Current lateral strafe input, -1..1 (issue #7). */
	UFUNCTION(BlueprintPure, Category = "Ship|Movement")
	float GetStrafe() const { return StrafeInput; }

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

	/** Max lateral thrust speed (uu/s) for evasive strafing (issue #7) — a dodge, not a drive, so
	 *  it's well below MaxSpeed. Scaled by engine power/damage like forward thrust. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Movement")
	float MaxStrafeSpeed = 950.f;

	/** How fast lateral speed ramps to target — punchy so a strafe reads as a thruster kick. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Movement")
	float StrafeAcceleration = 2600.f;

	/** Most negative throttle allowed — reverse thrust (issue #4). Reverse is a maneuvering burn to
	 *  back off incoming fire, so it's capped well short of full ahead (a fraction of MaxSpeed), not
	 *  a second forward gear. Set to 0 to disable reverse. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Movement")
	float ReverseThrottleMin = -0.35f;

protected:
	// --- Authoritative runtime state (replication-ready) ---
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ship|Movement")
	float ThrottleInput = 0.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ship|Movement")
	float TurnInput = 0.f;

	/** Lateral strafe input, -1 (port) .. 1 (starboard). */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ship|Movement")
	float StrafeInput = 0.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ship|Movement")
	float CurrentSpeed = 0.f;

	/** Eased lateral speed (uu/s), ramps toward StrafeInput * MaxStrafeSpeed. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ship|Movement")
	float CurrentStrafeSpeed = 0.f;

	/** While true, throttle/turn inputs are ignored and held at zero (docked, M19). */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ship|Movement")
	bool bInputLocked = false;

private:
	/** Lazily-resolved sibling power component (engine power scales MaxSpeed); may stay null. */
	UPROPERTY(Transient)
	TObjectPtr<class UPowerComponent> CachedPower;

	/** Lazily-resolved sibling damage control (M25); may stay null (enemies have none). */
	UPROPERTY(Transient)
	TObjectPtr<class UDamageControlComponent> CachedDamage;

	/** Engine power multiplier from the power component, or 1.0 if there is none. */
	float EnginePowerScale() const;

	/** Damaged-engine multiplier from damage control (0.5 while damaged), or 1.0 if none. */
	float EngineDamageScale() const;
};
