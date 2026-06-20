// SpaceGame — bridge simulator. Enemy ship pawn + simple AI (DECISIONS D5/D8).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "EnemyShip.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class URadarContactComponent;
class UHealthComponent;
class UMaterialInterface;

/** Coarse AI phase, surfaced for logging + [L] verification. */
UENUM(BlueprintType)
enum class EEnemyAIState : uint8
{
	Idle     UMETA(DisplayName = "Idle"),     // no player found
	Approach UMETA(DisplayName = "Approach"), // closing to standoff range
	Engage   UMETA(DisplayName = "Engage")    // in range, holding + firing
};

/**
 * AEnemyShip — a hostile ship with self-contained Tick AI (no AIController needed):
 * it finds the player pawn, yaws to face it, flies in to a standoff distance, and once
 * within engage range fires on a fixed interval (logged + drawn beam). Carries a
 * URadarContactComponent (so it shows on the tactical radar and is targetable by the
 * player's beam) and a UHealthComponent (so the player's beam can destroy it at M10).
 * Movement is applied directly to the transform — simple is fine for a vertical slice.
 */
UCLASS()
class SPACEGAME_API AEnemyShip : public APawn
{
	GENERATED_BODY()

public:
	AEnemyShip();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Enemy")
	EEnemyAIState GetAIState() const { return AIState; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	UHealthComponent* GetHealthComp() const { return HealthComp; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Enemy")
	TObjectPtr<USceneComponent> ShipRoot;

	UPROPERTY(VisibleAnywhere, Category = "Enemy")
	TObjectPtr<UStaticMeshComponent> ShipMesh;

	/** Twin forward prongs (M13 cruiser silhouette). */
	UPROPERTY(VisibleAnywhere, Category = "Enemy")
	TObjectPtr<UStaticMeshComponent> ProngLeft;

	UPROPERTY(VisibleAnywhere, Category = "Enemy")
	TObjectPtr<UStaticMeshComponent> ProngRight;

	/** Glowing red sensor "eye" at the bow + engine glow at the stern (emissive, M13). */
	UPROPERTY(VisibleAnywhere, Category = "Enemy")
	TObjectPtr<UStaticMeshComponent> SensorEye;

	UPROPERTY(VisibleAnywhere, Category = "Enemy")
	TObjectPtr<UStaticMeshComponent> EngineGlow;

	/** Radar blip + makes this a valid player weapon target. */
	UPROPERTY(VisibleAnywhere, Category = "Enemy")
	TObjectPtr<URadarContactComponent> RadarContact;

	/** Hull + shields; the player's beam drains this at M10. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<UHealthComponent> HealthComp;

	// --- AI tunables ---

	/** Forward cruise speed while approaching (uu/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AI")
	float MoveSpeed = 1100.f;

	/** Yaw rate while turning to face the player (deg/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AI")
	float TurnRateDeg = 50.f;

	/** Holds this far from the player once engaged (uu). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AI")
	float StandoffDistance = 6000.f;

	/** Begins firing once the player is within this range (uu). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AI")
	float EngageRange = 12000.f;

	/** Seconds between shots while engaged. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AI")
	float FireInterval = 2.5f;

	/** Raw damage per shot to the player (before the player's shield-power mitigation). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AI")
	float EnemyBeamDamage = 8.f;

	/** Emissive material for the enemy's beam + death explosion (M13 FX). */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> FxMaterial;

private:
	/** Bound to HealthComp->OnDeath: draw an explosion burst then despawn the ship. */
	UFUNCTION()
	void HandleDeath(AActor* DeadActor);

	/** Resolve the player's pawn (cached cheaply each call via GameplayStatics). */
	AActor* GetPlayerTarget() const;

	/** Move into the new AI phase, logging the transition once. */
	void SetAIState(EEnemyAIState NewState);

	/** Log + draw a shot at the player (damage to the player lands at M11). */
	void FireAtPlayer(const AActor* Target);

	EEnemyAIState AIState = EEnemyAIState::Idle;

	/** Counts down to the next shot while engaged. */
	float FireCooldown = 0.f;
};
