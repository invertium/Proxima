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
};
