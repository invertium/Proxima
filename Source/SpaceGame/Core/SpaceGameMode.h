// SpaceGame — bridge simulator. GameMode: wires the bridge player controller.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/StationTypes.h"
#include "SpaceGameMode.generated.h"

/**
 * ASpaceGameMode — sets ABridgePlayerController as the player controller. No
 * default pawn is spawned: the player's ship is placed in the level and
 * auto-possesses Player0 (DECISIONS D10, single-machine slice). Also holds the
 * encounter outcome phase (Playing/Victory/Defeat) that the bridge controller
 * sets and the LAN web stations read.
 */
UCLASS()
class SPACEGAME_API ASpaceGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASpaceGameMode();

	/** Current encounter outcome. Resets to Playing whenever the level (re)loads. */
	UFUNCTION(BlueprintPure, Category = "Game")
	EGamePhase GetPhase() const { return Phase; }

	/** Set by the bridge controller on defeat/victory. */
	UFUNCTION(BlueprintCallable, Category = "Game")
	void SetPhase(EGamePhase NewPhase);

	/** Reload the current level from scratch — a fresh encounter (New Game / Restart). */
	UFUNCTION(BlueprintCallable, Category = "Game")
	void RestartEncounter();

private:
	EGamePhase Phase = EGamePhase::Playing;
};
