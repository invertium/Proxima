// SpaceGame — bridge simulator. Subsystem damage — Engineering matters in combat (M25).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DamageControlComponent.generated.h"

class USoundBase;

/** Ship systems that can take combat damage (M25). Distinct from EShipSystem (the power rows):
 *  Sensors is damageable but draws no reactor power; Shields has power but no damage state. */
UENUM(BlueprintType)
enum class EDamageSystem : uint8
{
	Engine   UMETA(DisplayName = "Engine"),   // damaged: max speed halved
	Weapons  UMETA(DisplayName = "Weapons"),  // damaged: beam recharge halved
	Sensors  UMETA(DisplayName = "Sensors"),  // damaged: radar + scan range halved
	Count    UMETA(Hidden)
};

/**
 * UDamageControlComponent — tracks combat damage to the player ship's systems. Every hull hit
 * (shields already down) rolls DamageChance to knock out one working system; a damaged system
 * runs at DamagedMultiplier until Engineering repairs it. The web console's weld sweep (M17.3)
 * retargets to the damaged system: WeldsPerSystemRepair credited welds fix it, and only then do
 * further welds restore hull. Consumers (movement/weapon/science/radar/web state) read
 * GetMultiplier per system. Damage fires the bridge alarm and is flagged amber on the
 * Engineering console + web page + /api/state.
 */
UCLASS(ClassGroup = (SpaceGame), meta = (BlueprintSpawnableComponent))
class SPACEGAME_API UDamageControlComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDamageControlComponent();

	UFUNCTION(BlueprintPure, Category = "Ship|Damage")
	bool IsDamaged(EDamageSystem System) const;

	/** Stat multiplier for a system: DamagedMultiplier while damaged, else 1. */
	UFUNCTION(BlueprintPure, Category = "Ship|Damage")
	float GetMultiplier(EDamageSystem System) const
	{
		return IsDamaged(System) ? DamagedMultiplier : 1.f;
	}

	UFUNCTION(BlueprintPure, Category = "Ship|Damage")
	int32 GetDamagedCount() const;

	/** Knock out a system (no-op if already damaged). Also the scripted-test entry point. */
	UFUNCTION(BlueprintCallable, Category = "Ship|Damage")
	void DamageSystem(EDamageSystem System);

	/** Instantly restore a system (station dock repair-all, tests). */
	UFUNCTION(BlueprintCallable, Category = "Ship|Damage")
	void RepairSystem(EDamageSystem System);

	/** Restore every system (docking repairs the whole ship, M19). */
	UFUNCTION(BlueprintCallable, Category = "Ship|Damage")
	void RepairAll();

	/** One credited weld from the Engineering sweep. If a system is damaged the weld advances
	 *  its repair (WeldsPerSystemRepair welds fix it) and returns true — the weld is consumed
	 *  and must NOT also restore hull. Returns false when nothing is damaged. */
	UFUNCTION(BlueprintCallable, Category = "Ship|Damage")
	bool CreditRepairWeld();

	/** The system the weld sweep is currently fixing (first damaged in enum order), or Count. */
	UFUNCTION(BlueprintPure, Category = "Ship|Damage")
	EDamageSystem GetRepairTarget() const;

	/** Display name for a system ("ENGINE"/"WEAPONS"/"SENSORS"). */
	static const TCHAR* SystemName(EDamageSystem System);

	/** Chance (0..1) that a hull hit damages a working system. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Damage")
	float DamageChance = 0.35f;

	/** Stat multiplier applied to a damaged system. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Damage")
	float DamagedMultiplier = 0.5f;

	/** Credited welds needed to repair one damaged system. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Damage")
	int32 WeldsPerSystemRepair = 3;

	/** Only sound the system-damage alarm when hull is at/below this fraction (matches the ship's
	 *  low-hull klaxon). Above it a knocked-out system flags amber silently — no beep per scratch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Damage")
	float AlarmHullFraction = 0.3f;

protected:
	virtual void BeginPlay() override;

	/** Bound to the sibling health's OnDamaged: rolls system damage on hull hits. */
	UFUNCTION()
	void HandleOwnerDamaged(float EffectiveDamage, float HullRemaining);

	/** Per-system damage flags, indexed by EDamageSystem. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ship|Damage")
	TArray<bool> Damaged;

	/** Welds credited toward the current repair target; resets when it's fixed. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ship|Damage")
	int32 RepairWelds = 0;

private:
	/** True when the owner's hull is at/below AlarmHullFraction (gates the system-damage beep). */
	bool IsHullCritical() const;

	/** Short alarm burst on system damage (shares the S_Alarm asset with the low-hull klaxon). */
	UPROPERTY()
	TObjectPtr<USoundBase> AlarmSound;
};
