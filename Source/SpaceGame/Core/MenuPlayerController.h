// SpaceGame — bridge simulator. Main-menu player controller (M18).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MenuPlayerController.generated.h"

class UMainMenuWidget;

/**
 * AMenuPlayerController — drives the MainMenu map: on BeginPlay it builds the C++ main-menu
 * widget, adds it to the viewport, and switches to UI input (cursor shown). No pawn is
 * possessed — the menu map is just a backdrop for the front-end.
 */
UCLASS()
class SPACEGAME_API AMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<UMainMenuWidget> Menu;
};
