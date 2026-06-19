// SpaceGame — bridge simulator. GameMode: wires the bridge player controller.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SpaceGameMode.generated.h"

/**
 * ASpaceGameMode — sets ABridgePlayerController as the player controller. No
 * default pawn is spawned: the player's ship is placed in the level and
 * auto-possesses Player0 (DECISIONS D10, single-machine slice).
 */
UCLASS()
class SPACEGAME_API ASpaceGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASpaceGameMode();
};
