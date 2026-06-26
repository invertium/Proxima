// SpaceGame — bridge simulator. Science sensors (M17 Science station).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ScienceComponent.generated.h"

class UHealthComponent;

/**
 * UScienceComponent — the Science officer's sensor suite. It cycles a scan target through the
 * radar contacts (independent of the Weapons lock), and a scan that takes ScanDuration to
 * complete. Once a contact is scanned, its hull/shield values are read live from the target's
 * UHealthComponent and surfaced to the Science console. Switching targets clears the scan.
 */
UCLASS(ClassGroup = (SpaceGame), meta = (BlueprintSpawnableComponent))
class SPACEGAME_API UScienceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UScienceComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Cycle the scan target to the next radar contact (wraps); clears any scan in progress. */
	UFUNCTION(BlueprintCallable, Category = "Ship|Science")
	void CycleTarget();

	/** Begin scanning the current target (no-op if none, or already scanned). */
	UFUNCTION(BlueprintCallable, Category = "Ship|Science")
	void BeginScan();

	UFUNCTION(BlueprintPure, Category = "Ship|Science")
	AActor* GetScanTarget() const { return ScanTarget; }

	/** Scan progress 0..1 (1 = complete/revealed). */
	UFUNCTION(BlueprintPure, Category = "Ship|Science")
	float GetScanProgress() const { return ScanProgress; }

	UFUNCTION(BlueprintPure, Category = "Ship|Science")
	bool IsScanning() const { return bScanning; }

	UFUNCTION(BlueprintPure, Category = "Ship|Science")
	bool IsScanned() const { return bScanned; }

	// Revealed live values (−1 until a scan completes on a target with health).
	UFUNCTION(BlueprintPure, Category = "Ship|Science")
	float GetTargetHull() const;

	UFUNCTION(BlueprintPure, Category = "Ship|Science")
	float GetTargetMaxHull() const;

	UFUNCTION(BlueprintPure, Category = "Ship|Science")
	float GetTargetShield() const;

	UFUNCTION(BlueprintPure, Category = "Ship|Science")
	float GetTargetMaxShield() const;

	/** Seconds a full scan takes once started. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Science")
	float ScanDuration = 2.5f;

protected:
	// --- Authoritative runtime state (replication-ready, D7) ---
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ship|Science")
	TObjectPtr<AActor> ScanTarget;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ship|Science")
	float ScanProgress = 0.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ship|Science")
	bool bScanning = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ship|Science")
	bool bScanned = false;

private:
	/** Resolve the scanned target's health component, or null. */
	UHealthComponent* TargetHealth() const;
};
