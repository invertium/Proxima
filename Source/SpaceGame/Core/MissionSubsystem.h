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

/** One campaign mission: a name, the enemy fleet to field, and its comms script. */
USTRUCT()
struct FMissionDef
{
	GENERATED_BODY()

	UPROPERTY() FString Name;
	UPROPERTY() TArray<EEnemyType> Enemies;
	UPROPERTY() TArray<FCommsBeat> Comms;
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

	/** The campaign table size (kept in sync with USpaceGameInstance::GetMissionCount). */
	static int32 MissionCount();

	/** Look up a mission definition by index (clamped to the table). */
	static FMissionDef GetMissionDef(int32 Index);

private:
	/** Destroy level-placed hostiles and spawn this mission's fleet ahead of the player. */
	void BuildEncounter(UWorld& World);

	int32 MissionIndex = 0;
	FMissionDef Mission;
};
