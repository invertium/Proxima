// SpaceGame — bridge simulator. Engineering power model (DECISIONS D11).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PowerComponent.generated.h"

/** Ship systems that draw power. Each maps to one Engineering row + one stat. */
UENUM(BlueprintType)
enum class EShipSystem : uint8
{
	Engine   UMETA(DisplayName = "Engine"),  // scales impulse MaxSpeed
	Weapons  UMETA(DisplayName = "Weapons"), // scales beam recharge (M8)
	Shields  UMETA(DisplayName = "Shields"), // scales damage mitigation (M9+)
	Count    UMETA(Hidden)
};

/**
 * UPowerComponent — EmptyEpsilon-style per-system power (DECISIONS D11). Each system
 * holds a power level 0..MaxPerSystem (1.0 = nominal 100%). Engineering reallocates
 * power between systems; other subsystems read their level as a multiplier
 * (engine→MaxSpeed now; weapons/shields at M8/M9). State is plain UPROPERTYs so it
 * stays replication-ready (DECISIONS D7). Reactor budget is a soft readout for the
 * slice — levels are clamped per-system, not hard-summed.
 */
UCLASS(ClassGroup = (SpaceGame), meta = (BlueprintSpawnableComponent))
class SPACEGAME_API UPowerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPowerComponent();

	/** Power multiplier for a system (0..MaxPerSystem; 1.0 = nominal). */
	UFUNCTION(BlueprintPure, Category = "Ship|Power")
	float GetSystemPower(EShipSystem System) const;

	/** Set a system's power level directly (clamped 0..MaxPerSystem). */
	UFUNCTION(BlueprintCallable, Category = "Ship|Power")
	void SetSystemPower(EShipSystem System, float Value);

	/** Nudge a system's power by Delta (clamped 0..MaxPerSystem). */
	UFUNCTION(BlueprintCallable, Category = "Ship|Power")
	void AdjustSystemPower(EShipSystem System, float Delta);

	/** Sum of all system power, for the reactor-load readout. */
	UFUNCTION(BlueprintPure, Category = "Ship|Power")
	float GetTotalPower() const;

	/** Ceiling for any single system (nominal = 1.0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Power")
	float MaxPerSystem = 2.0f;

	/** Nominal reactor budget (sum at all-nominal = SystemCount * 1.0); soft, for display. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Power")
	float ReactorBudget = 3.0f;

protected:
	/** Per-system power, indexed by EShipSystem; initialised to nominal in the ctor. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ship|Power")
	TArray<float> SystemPower;
};
