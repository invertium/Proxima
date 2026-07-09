// SpaceGame — bridge simulator. Mission/encounter builder (M18).

#pragma once

#include "CoreMinimal.h"
#include "Core/StationTypes.h"
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

/** A rolling sector travel event (M27): at most one live at a time, announced on comms. */
UENUM(BlueprintType)
enum class ESectorEvent : uint8
{
	None         UMETA(DisplayName = "None"),
	Distress     UMETA(DisplayName = "Distress Call"),  // timed: clear raiders near another system
	Interdiction UMETA(DisplayName = "Interdiction"),   // pirate ambush on the ship's path
	Salvage      UMETA(DisplayName = "Salvage Cache")   // free-floating pickup, collect by proximity
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
 * UMissionSubsystem — the open-sector director (M23). The sector level loads once; the director
 * spawns every system's landmark and the home starbase at begin play, then drives the campaign
 * *in place* with no per-mission level reloads: on a short timer it watches the player, and when
 * the ship flies within TriggerRadius of the active objective's landmark it spawns that mission's
 * fleet around the body and opens its comms script. When the fleet is wiped it advances the
 * campaign, fires a "next objective" beat, and re-arms for the next system — seamlessly. The final
 * clear hands off to the controller for the campaign epilogue. Holds the running comms log for the
 * Science console.
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

	/** The scripted transmissions fired so far this campaign (oldest first), for the Science console. */
	const TArray<FCommsMessage>& GetComms() const { return CommsLog; }

	/** Append an ad-hoc transmission to the comms log (M26: e.g. Science's weakpoint confirm). */
	void PostComms(const FString& Sender, const FString& Text);

	/** The campaign table size (kept in sync with USpaceGameInstance::GetMissionCount). */
	static int32 MissionCount();

	/** Look up a mission definition by index (clamped to the table). */
	static FMissionDef GetMissionDef(int32 Index);

	/** World location of a system's landmark in the open sector (M23): the home system sits at the
	 *  sector anchor (player start) and the rest spread out by MapX/MapY. */
	UFUNCTION(BlueprintPure, Category = "Mission")
	FVector GetSystemLocation(int32 Index) const;

	/** Fold a world position back into the normalised (0..1) sector-map space (for the live nav map). */
	UFUNCTION(BlueprintPure, Category = "Mission")
	FVector2D GetMapPosition(FVector World) const;

	/** True once an active mission's fleet is spawned and the fight is on (proximity-triggered). */
	UFUNCTION(BlueprintPure, Category = "Mission")
	bool IsEncounterLive() const { return bEncounterLive; }

	/** True once the final system has been cleared (the campaign is won). */
	UFUNCTION(BlueprintPure, Category = "Mission")
	bool IsSectorComplete() const { return bSectorComplete; }

	/** Display name of the current objective (its landmark, falling back to the mission name). */
	UFUNCTION(BlueprintPure, Category = "Mission")
	FString GetObjectiveName() const;

	/** World location of the active objective's landmark — where the crew must fly to trigger it. */
	UFUNCTION(BlueprintPure, Category = "Mission")
	FVector GetActiveObjectiveLocation() const { return GetSystemLocation(MissionIndex); }

	/** Planar distance (uu) from the player to the active objective (-1 if unknown / complete). */
	UFUNCTION(BlueprintPure, Category = "Mission")
	float GetObjectiveDistance() const;

	/** Force the active objective's fleet to spawn now, ignoring proximity (debug / skip-ahead). */
	UFUNCTION(BlueprintCallable, Category = "Mission")
	void ForceTriggerObjective();

	/** The live travel event (M27), if any. */
	UFUNCTION(BlueprintPure, Category = "Mission")
	ESectorEvent GetActiveEvent() const { return ActiveEvent; }

	/** World location of the live event (its fleet anchor / cargo pod). */
	UFUNCTION(BlueprintPure, Category = "Mission")
	FVector GetEventLocation() const { return EventLocation; }

	/** Seconds until the live event expires (-1 when none). */
	UFUNCTION(BlueprintPure, Category = "Mission")
	float GetEventTimeLeft() const;

	/** Start a specific travel event now, skipping the periodic roll (debug / API). */
	UFUNCTION(BlueprintCallable, Category = "Mission")
	void ForceEvent(ESectorEvent Kind);

	/** Wire name for an event kind ("distress"/"interdiction"/"salvage"), for the nav-map JSON. */
	static const TCHAR* EventKindName(ESectorEvent Kind);

	// --- Station contracts (M28): offered while docked, tracked by the director ---

	/** The contract on offer at the starbase board (regenerated each docking; None when
	 *  a contract is already active or the ship is not docked). */
	UFUNCTION(BlueprintPure, Category = "Mission")
	EContractType GetOfferType() const { return OfferType; }

	UFUNCTION(BlueprintPure, Category = "Mission")
	int32 GetOfferTargetA() const { return OfferTargetA; }

	UFUNCTION(BlueprintPure, Category = "Mission")
	int32 GetOfferTargetB() const { return OfferTargetB; }

	UFUNCTION(BlueprintPure, Category = "Mission")
	const FString& GetOfferShip() const { return OfferShip; }

	UFUNCTION(BlueprintPure, Category = "Mission")
	int32 GetOfferReward() const { return OfferReward; }

	/** One-line description of a contract for the board / comms. */
	FString DescribeContract(EContractType Type, int32 A, int32 B, const FString& Ship, int32 Reward) const;

	/** Sign the offered contract (must be docked, offer live, no active contract). Persists,
	 *  announces on comms, and spawns the bounty target when applicable. Returns true on success. */
	UFUNCTION(BlueprintCallable, Category = "Mission")
	bool AcceptContract();

	/** Wire name for a contract type ("bounty"/"patrol"/"delivery"). */
	static const TCHAR* ContractTypeName(EContractType Type);

private:
	/** Spawn the friendly starbase just behind the player's start (once per encounter). */
	void SpawnStation(UWorld& World);

	/** Spawn every system's landmark body across the open sector (M23). */
	void SpawnLandmarks(UWorld& World);

	/** Load the objective's def, reset its script, and fire its briefing (where to fly + why). */
	void BeginObjective(int32 Index);

	/** Director tick (repeating timer): trigger the active fleet once the player enters its zone. */
	void CheckDirector();

	/** Spawn the active mission's fleet around its landmark and open the fight. */
	void TriggerActiveEncounter();

	/** Called when the live fleet is wiped: advance + save, fire the clear beat, re-arm or finish. */
	void OnFleetCleared();

	/** Destroy level-placed hostiles and spawn the active fleet around its landmark, facing the player. */
	void SpawnFleet(UWorld& World);

	/** How many of the currently-tracked fleet are still alive. */
	int32 CountLiveFleet() const;

	/** Poll for time-triggered comms beats (repeating timer; paused with the game). */
	void CheckTimedBeats();

	/** Bound to each spawned hostile's death: advance the kill count + fire kill-triggered beats. */
	UFUNCTION()
	void HandleEnemyKilled(AActor* DeadActor);

	// --- M27 travel events ---

	/** Periodic roll: while not engaged and nothing live, maybe start a travel event. */
	void RollEvent();

	/** Spin up an event: spawn its ships/pod, set the deadline, announce it on comms. */
	void StartEvent(ESectorEvent Kind);

	/** Event tick (from CheckDirector): expiry countdown + salvage proximity collection. */
	void CheckEvent();

	/** Close out the live event: payout + comms on success, cleanup + comms on expiry. */
	void ResolveEvent(bool bSuccess);

	/** Spawn a small hostile group for an event at Center (not part of the campaign fleet). */
	void SpawnEventShips(UWorld& World, const FVector& Center, const TArray<EEnemyType>& Types);

	/** How many of the event's ships are still alive. */
	int32 CountEventFleetAlive() const;

	/** Bound to each event ship's death: resolves the event once its group is wiped. */
	UFUNCTION()
	void HandleEventShipKilled(AActor* DeadActor);

	// --- M28 station contracts ---

	/** Contract tick (from CheckDirector): dock-edge offer refresh + waypoint/return progress. */
	void CheckContract();

	/** Roll a fresh offer for the board (called on each docking with no active contract). */
	void GenerateOffer();

	/** Spawn the active bounty contract's loitering target at its landmark (accept + reload). */
	void SpawnBountyShip();

	/** Pay out + clear the active contract, with a comms confirm. */
	void CompleteContract();

	/** Bound to the bounty target's death. */
	UFUNCTION()
	void HandleBountyKilled(AActor* DeadActor);

	/** Append a beat to the comms log (once). */
	void FireBeat(FCommsBeat& Beat);

	int32 MissionIndex = 0;   // the active objective index (== USpaceGameInstance::GetMissionIndex).
	FMissionDef Mission;      // the active objective's definition.

	/** Sector origin = the player's begin-play location; the home system sits here (M23). */
	FVector SectorAnchor = FVector::ZeroVector;

	/** The fleet spawned for the live encounter (tracked to detect a clear). */
	TArray<TWeakObjectPtr<AEnemyShip>> LiveFleet;

	TArray<FCommsMessage> CommsLog;
	float StartTime = 0.f;
	int32 KillCount = 0;
	bool bEncounterLive = false;   // an active fleet is up and the fight is on.
	bool bSectorComplete = false;  // the final system has been cleared.
	FTimerHandle BeatTimer;
	FTimerHandle DirectorTimer;

	/** Planar range (uu) from the objective's landmark at which its fleet triggers. First-pass tunable. */
	UPROPERTY(EditAnywhere, Category = "Mission")
	float TriggerRadius = 18000.f;

	// --- M27 travel-event state ---

	ESectorEvent ActiveEvent = ESectorEvent::None;
	FVector EventLocation = FVector::ZeroVector;
	double EventDeadline = 0.0;
	TArray<TWeakObjectPtr<AEnemyShip>> EventFleet;
	TWeakObjectPtr<class ASalvageCache> SalvagePod;
	FTimerHandle EventRollTimer;

	// --- M28 contract state ---

	/** The board's current offer (valid only while docked with no active contract). */
	EContractType OfferType = EContractType::None;
	int32 OfferTargetA = -1;
	int32 OfferTargetB = -1;
	FString OfferShip;
	int32 OfferReward = 0;

	/** Dock-edge detector for regenerating the board's offer. */
	bool bWasDocked = false;

	/** The live bounty target, so reloads don't double-spawn it. */
	TWeakObjectPtr<AEnemyShip> BountyShip;

	/** Planar range (uu) that counts as "visiting" a contract waypoint. */
	UPROPERTY(EditAnywhere, Category = "Mission|Contracts")
	float ContractVisitRange = 9000.f;

	// M27 tunables (first pass): roll cadence + odds, per-event lifetimes, payouts.
	UPROPERTY(EditAnywhere, Category = "Mission|Events") float EventRollInterval = 25.f;
	UPROPERTY(EditAnywhere, Category = "Mission|Events") float EventChance = 0.4f;
	UPROPERTY(EditAnywhere, Category = "Mission|Events") float DistressDuration = 150.f;
	UPROPERTY(EditAnywhere, Category = "Mission|Events") float InterdictionDuration = 180.f;
	UPROPERTY(EditAnywhere, Category = "Mission|Events") float SalvageDuration = 120.f;
	UPROPERTY(EditAnywhere, Category = "Mission|Events") float SalvageCollectRange = 1500.f;
	UPROPERTY(EditAnywhere, Category = "Mission|Events") int32 DistressBonusCredits = 150;
	UPROPERTY(EditAnywhere, Category = "Mission|Events") int32 InterdictionBonusCredits = 60;
	UPROPERTY(EditAnywhere, Category = "Mission|Events") int32 SalvageCredits = 90;
};
