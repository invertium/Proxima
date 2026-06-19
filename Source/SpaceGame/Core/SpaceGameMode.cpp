// SpaceGame — bridge simulator. GameMode implementation.

#include "Core/SpaceGameMode.h"

#include "Core/BridgePlayerController.h"

ASpaceGameMode::ASpaceGameMode()
{
	PlayerControllerClass = ABridgePlayerController::StaticClass();

	// The player's ship is placed in the level and auto-possesses Player0;
	// don't spawn a DefaultPawn over it.
	DefaultPawnClass = nullptr;
}
