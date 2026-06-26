// SpaceGame — bridge simulator. Main-menu game mode (M18).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MenuGameMode.generated.h"

/**
 * AMenuGameMode — the GameMode for the MainMenu map. Uses the menu player controller and
 * spawns no pawn; the map is just a backdrop behind the front-end widget. Set as the
 * MainMenu map's World Settings GameMode override.
 */
UCLASS()
class SPACEGAME_API AMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMenuGameMode();
};
