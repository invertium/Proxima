// SpaceGame — bridge simulator. Engineering power model (DECISIONS D11).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PowerComponent.generated.h"

/** One-tap Engineering power doctrines (M29): a full Engine/Weapons/Shields triple each. */
UENUM(BlueprintType)
enum class EPowerPreset : uint8
{
	Balanced UMETA(DisplayName = "Balanced"), // 1.0 / 1.0 / 1.0
	Combat   UMETA(DisplayName = "Combat"),   // guns + shields up, engines trimmed
	Travel   UMETA(DisplayName = "Travel")    // engines redlined, everything else trimmed
};

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
 * stays replication-ready (DECISIONS D7). ReactorBudget is a hard cap on the *total*:
 * the reactor can only supply so much, so boosting one system requires taking power
 * from another — power is zero-sum within the budget (no free 200%-everything).
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

	/** Snap all three systems to a doctrine's triple in one tap (M29). Stays within budget. */
	UFUNCTION(BlueprintCallable, Category = "Ship|Power")
	void ApplyPreset(EPowerPreset Preset);

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
