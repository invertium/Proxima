// SpaceGame — bridge simulator. Main-menu player controller implementation.

#include "Core/MenuPlayerController.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Core/MainMenuWidget.h"

void AMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	Menu = CreateWidget<UMainMenuWidget>(this, UMainMenuWidget::StaticClass());
	if (Menu)
	{
		Menu->AddToViewport(100);
	}

	bShowMouseCursor = true;
	UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(this, Menu, EMouseLockMode::DoNotLock);
}
