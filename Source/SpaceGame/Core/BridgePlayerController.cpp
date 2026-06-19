// SpaceGame — bridge simulator. Player controller implementation.

#include "Core/BridgePlayerController.h"

#include "Core/BridgeHUDWidget.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"

void ABridgePlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Keys reach the controller while the full-screen HUD overlay is shown.
	bShowMouseCursor = false;

	// Fall back to the authored WBP if no class was assigned in defaults. Resolved
	// at play time (not CDO) so creating the WBP doesn't require an editor restart.
	if (!HUDWidgetClass)
	{
		HUDWidgetClass = LoadClass<UBridgeHUDWidget>(
			nullptr, TEXT("/Game/UI/Common/WBP_BridgeHUD.WBP_BridgeHUD_C"));
	}

	if (HUDWidgetClass)
	{
		HUDWidget = CreateWidget<UBridgeHUDWidget>(this, HUDWidgetClass);
		if (HUDWidget)
		{
			HUDWidget->AddToViewport();
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Bridge] HUDWidgetClass not set on %s — no HUD shell."), *GetName());
	}

	SetStation(CurrentStation);
}

void ABridgePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Legacy key binds: three number keys, no Input Action assets needed (D9 primitive-first).
	InputComponent->BindKey(EKeys::One,   IE_Pressed, this, &ABridgePlayerController::SelectHelm);
	InputComponent->BindKey(EKeys::Two,   IE_Pressed, this, &ABridgePlayerController::SelectWeapons);
	InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ABridgePlayerController::SelectEngineering);
}

void ABridgePlayerController::SetStation(EStation NewStation)
{
	CurrentStation = NewStation;

	const TCHAR* Name =
		NewStation == EStation::Weapons     ? TEXT("Weapons") :
		NewStation == EStation::Engineering ? TEXT("Engineering") :
		                                      TEXT("Helm");
	UE_LOG(LogTemp, Log, TEXT("[Bridge] Active station -> %s"), Name);

	if (HUDWidget)
	{
		HUDWidget->SetActiveStation(NewStation);
	}
}
