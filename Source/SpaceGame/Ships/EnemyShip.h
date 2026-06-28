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
class USoundBase;

/** Coarse AI phase, surfaced for logging + [L] verification. */
UENUM(BlueprintType)
enum class EEnemyAIState : uint8
{
	Idle     UMETA(DisplayName = "Idle"),     // no player found
	Approach UMETA(DisplayName = "Approach"), // closing to standoff range
	Engage   UMETA(DisplayName = "Engage")    // in range, holding + firing
};

/**
 * Enemy archetype (M18). Each preset (applied in BeginPlay) reskins + rescales the ship and
 * tunes its AI/health stats, so a mission can field a varied fleet from the two existing meshes.
 */
UENUM(BlueprintType)
enum class EEnemyType : uint8
{
	Gunship UMETA(DisplayName = "Gunship"), // baseline Imperial cruiser (orange)
	Scout   UMETA(DisplayName = "Scout"),   // small/fast/fragile Insurgent hull (cyan)
	Cruiser UMETA(DisplayName = "Cruiser")  // big/slow/tanky, hits hard (red)
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

	/** Radio callsign shown on the crew consoles (e.g. "VIPER-2"). Falls back to a type-based
	 *  default in BeginPlay if the mission spawner didn't assign one. */
	UFUNCTION(BlueprintPure, Category = "Enemy")
	const FString& GetCallsign() const { return Callsign; }

	/** Build a flavourful callsign for an archetype + per-type ordinal (e.g. Scout #1 -> "WASP-1"). */
	static FString MakeCallsign(EEnemyType Type, int32 OrdinalOfType);

	/** Salvage paid to the campaign wallet when this ship is destroyed (set per type in BeginPlay). */
	UFUNCTION(BlueprintPure, Category = "Enemy")
	int32 GetRewardCredits() const { return RewardCredits; }

	/** XP awarded for the kill (drives the campaign Rank; set per type in BeginPlay). */
	UFUNCTION(BlueprintPure, Category = "Enemy")
	int32 GetRewardXP() const { return RewardXP; }

	/** Override the post-spawn fire-hold (seconds) before BeginPlay reads it into GraceTimer. The
	 *  mission spawner uses this to field a passive target drone for the tutorial (huge delay). */
	UFUNCTION(BlueprintCallable, Category = "Enemy|AI")
	void SetEngageDelay(float Seconds) { EngageDelay = Seconds; }

	/** Archetype, set by the mission spawner before FinishSpawning; applied in BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
	EEnemyType ShipType = EEnemyType::Gunship;

	/** Callsign, set by the mission spawner before FinishSpawning (or defaulted in BeginPlay). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
	FString Callsign;

protected:
	virtual void BeginPlay() override;

	/** Reskin/rescale + retune this ship for ShipType (mesh, material, scale, AI + health stats). */
	void ApplyTypePreset();

	UPROPERTY(VisibleAnywhere, Category = "Enemy")
	TObjectPtr<USceneComponent> ShipRoot;

	/** Main hull — imported Quaternius "Imperial" cruiser mesh (CC0). */
	UPROPERTY(VisibleAnywhere, Category = "Enemy")
	TObjectPtr<UStaticMeshComponent> ShipMesh;

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

	/** Grace period after spawn before this ship opens fire (uu): a beat for the player to get
	 *  oriented at mission start. The ship still closes in during it; it just holds its fire. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AI")
	float EngageDelay = 6.f;

	/** Raw damage per shot to the player (before the player's shield-power mitigation). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AI")
	float EnemyBeamDamage = 8.f;

	/** Salvage credits + XP this kill is worth (set per archetype in ApplyTypePreset). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Reward")
	int32 RewardCredits = 80;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Reward")
	int32 RewardXP = 30;

	/** Emissive material for the enemy's beam + death explosion (M13 FX). */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> FxMaterial;

	/** Enemy beam-fire SFX (CC0), loaded in the constructor; played at the enemy's location. */
	UPROPERTY()
	TObjectPtr<USoundBase> FireSound;

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

	/** Counts down from EngageDelay after spawn; while > 0 the ship holds its fire. */
	float GraceTimer = 0.f;
};
