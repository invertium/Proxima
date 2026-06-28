// SpaceGame — bridge simulator. Mission/encounter builder (M18).

#pragma once

#include "CoreMinimal.h"
#include "Ships/EnemyShip.h"
#include "Subsystems/WorldSubsystem.h"
#include "MissionSubsystem.generated.h"

/** A scripted comms beat shown on the Science console (M18.6 fills these in). */
USTRUCT()
struct FCommsBeat
{
	GENERATED_BODY()

	UPROPERTY() FString Sender;
	UPROPERTY() FString Text;
	UPROPERTY() float AtSeconds = 0.f;   // delay after mission start (for timed beats)
	UPROPERTY() int32 OnKill = -1;       // fire after this many kills (-1 = time-only)
	bool bFired = false;
};

/** Visual archetype for a system's landmark on the open sector map (M23). */
UENUM(BlueprintType)
enum class ELandmarkKind : uint8
{
	Planet UMETA(DisplayName = "Planet"), // a modest coloured world
	Sun    UMETA(DisplayName = "Sun")     // a huge bright star
};

/** One campaign mission: a name, the enemy fleet to field, and its comms script. */
USTRUCT()
struct FMissionDef
{
	GENERATED_BODY()

	UPROPERTY() FString Name;
	UPROPERTY() TArray<EEnemyType> Enemies;
	UPROPERTY() TArray<FCommsBeat> Comms;

	/** Pre-fight briefing shown on the comms log during the staging phase (before launch). Empty =
	 *  fall back to the generic "sector clear, resupply + launch" prompt. */
	UPROPERTY() FString BriefSender;
	UPROPERTY() FString BriefText;

	/** Position of this system on the sector starmap (M21), normalised 0..1 in each axis. */
	UPROPERTY() float MapX = 0.5f;
	UPROPERTY() float MapY = 0.5f;

	/** The system's landmark on the open sector (M23): a distinct body the crew flies to. */
	UPROPERTY() FString LandmarkName;
	UPROPERTY() ELandmarkKind LandmarkKind = ELandmarkKind::Planet;
	UPROPERTY() FLinearColor LandmarkColor = FLinearColor(0.4f, 0.7f, 1.0f, 1.0f);
	UPROPERTY() float LandmarkScale = 1.0f;

	/** If >= 0, every spawned hostile holds fire this many seconds (tutorial uses a huge value to
	 *  field a passive target drone). -1 keeps each archetype's own EngageDelay. */
	UPROPERTY() float EngageDelayOverride = -1.f;

	/** When true the fleet spawns immediately (no staging) — used by the tutorial. Combat missions
	 *  leave this false so they open in a staging phase: dock + outfit, then launch when ready. */
	UPROPERTY() bool bAutoLaunch = false;
};

/** A transmission shown on the Science comms log. */
USTRUCT()
struct FCommsMessage
{
	GENERATED_BODY()

	UPROPERTY() FString Sender;
	UPROPERTY() FString Text;
};

/**
 * UMissionSubsystem — builds the encounter for the current campaign mission. On world begin
 * play it reads the mission index from the game instance, clears any level-placed hostiles,
 * and spawns the mission's fleet in a fan ahead of the player (so missions are data, not maps).
 * It also re-binds the controller's win condition to the freshly spawned ships. Holds the
 * mission's comms script for the Science console (driven in M18.6).
 */
UCLASS()
class SPACEGAME_API UMissionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	UFUNCTION(BlueprintPure, Category = "Mission")
	int32 GetMissionIndex() const { return MissionIndex; }

	UFUNCTION(BlueprintPure, Category = "Mission")
	FString GetMissionName() const { return Mission.Name; }

	/** The scripted transmissions fired so far this mission (oldest first), for the Science console. */
	const TArray<FCommsMessage>& GetComms() const { return CommsLog; }

	/** The campaign table size (kept in sync with USpaceGameInstance::GetMissionCount). */
	static int32 MissionCount();

	/** Look up a mission definition by index (clamped to the table). */
	static FMissionDef GetMissionDef(int32 Index);

	/** World location of a system's landmark in the open sector (M23): the home system sits at the
	 *  sector anchor (player start) and the rest spread out by MapX/MapY. */
	UFUNCTION(BlueprintPure, Category = "Mission")
	FVector GetSystemLocation(int32 Index) const;

	/** True while the encounter is staged: the sector is clear (no fleet yet) so the crew can dock,
	 *  repair, and outfit before the fight. Launch spawns the fleet and clears this. */
	UFUNCTION(BlueprintPure, Category = "Mission")
	bool IsStaged() const { return bStaged; }

	/** Begin the engagement: spawn the mission's fleet and start its comms script. No-op once launched
	 *  (or while already fighting). Triggered from the Helm "LAUNCH" control. */
	UFUNCTION(BlueprintCallable, Category = "Mission")
	void LaunchEncounter();

private:
	/** Spawn the friendly starbase just behind the player's start (once per encounter). */
	void SpawnStation(UWorld& World);

	/** Spawn every system's landmark body across the open sector (M23). */
	void SpawnLandmarks(UWorld& World);

	/** Destroy level-placed hostiles and spawn this mission's fleet ahead of the player. */
	void SpawnFleet(UWorld& World);

	/** Poll for time-triggered comms beats (repeating timer; paused with the game). */
	void CheckTimedBeats();

	/** Bound to each spawned hostile's death: advance the kill count + fire kill-triggered beats. */
	UFUNCTION()
	void HandleEnemyKilled(AActor* DeadActor);

	/** Append a beat to the comms log (once). */
	void FireBeat(FCommsBeat& Beat);

	int32 MissionIndex = 0;
	FMissionDef Mission;

	/** Sector origin = the player's begin-play location; the home system sits here (M23). */
	FVector SectorAnchor = FVector::ZeroVector;

	TArray<FCommsMessage> CommsLog;
	float StartTime = 0.f;
	int32 KillCount = 0;
	bool bStaged = false;
	bool bLaunched = false;
	FTimerHandle BeatTimer;
};
