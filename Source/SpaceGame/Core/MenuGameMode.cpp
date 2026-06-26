// SpaceGame — bridge simulator. Main-menu game mode implementation.

#include "Core/MenuGameMode.h"

#include "Core/MenuPlayerController.h"

AMenuGameMode::AMenuGameMode()
{
	PlayerControllerClass = AMenuPlayerController::StaticClass();
	DefaultPawnClass = nullptr; // front-end only; no ship in the menu
}
