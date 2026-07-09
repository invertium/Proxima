// SpaceGame — bridge simulator. Persistent campaign save (M18).

#pragma once

#include "CoreMinimal.h"
#include "Core/StationTypes.h"
#include "GameFramework/SaveGame.h"
#include "SpaceSaveGame.generated.h"

/**
 * USpaceSaveGame — the on-disk campaign record. Mirrors the campaign fields held live on
 * USpaceGameInstance; written/read with UGameplayStatics::SaveGameToSlot/LoadGameFromSlot.
 * Campaign-progress only (reached mission + chosen ship) — encounters start fresh, so there
 * is no mid-fight snapshot to serialise.
 */
UCLASS()
class SPACEGAME_API USpaceSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** Highest mission the player has reached (0-based index into the campaign table). */
	UPROPERTY(VisibleAnywhere, Category = "Campaign")
	int32 MissionIndex = 0;

	/** Ship the campaign is being played with. */
	UPROPERTY(VisibleAnywhere, Category = "Campaign")
	EPlayerShipType PlayerShip = EPlayerShipType::Interceptor;

	/** Banked salvage credits (M19 progression economy). */
	UPROPERTY(VisibleAnywhere, Category = "Campaign")
	int32 Credits = 0;

	/** Lifetime XP (drives crew rank). */
	UPROPERTY(VisibleAnywhere, Category = "Campaign")
	int32 XP = 0;

	/** Owned drydock upgrade tiers keyed by catalogue id (M19.3). */
	UPROPERTY(VisibleAnywhere, Category = "Campaign")
	TMap<FName, int32> UpgradeTiers;

	/** Bought non-starter ships (M19.4). */
	UPROPERTY(VisibleAnywhere, Category = "Campaign")
	TArray<EPlayerShipType> OwnedShips;

	/** Campaign difficulty (M30). */
	UPROPERTY(VisibleAnywhere, Category = "Campaign")
	EDifficulty Difficulty = EDifficulty::Captain;

	// --- Active station contract (M28); None = no contract running ---

	UPROPERTY(VisibleAnywhere, Category = "Campaign")
	EContractType ContractType = EContractType::None;

	/** Primary target system index (bounty location / patrol leg 1 / delivery destination). */
	UPROPERTY(VisibleAnywhere, Category = "Campaign")
	int32 ContractTargetA = -1;

	/** Secondary target system index (patrol leg 2; unused otherwise). */
	UPROPERTY(VisibleAnywhere, Category = "Campaign")
	int32 ContractTargetB = -1;

	/** Progress stage (0 = fresh; patrol/delivery advance to 1 at the first waypoint). */
	UPROPERTY(VisibleAnywhere, Category = "Campaign")
	int32 ContractStage = 0;

	/** Bounty ship's callsign (e.g. "KRAIT"). */
	UPROPERTY(VisibleAnywhere, Category = "Campaign")
	FString ContractShip;

	/** Credits paid on completion. */
	UPROPERTY(VisibleAnywhere, Category = "Campaign")
	int32 ContractReward = 0;
};
