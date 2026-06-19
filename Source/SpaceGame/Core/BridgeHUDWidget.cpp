// SpaceGame — bridge simulator. HUD shell widget implementation.

#include "Core/BridgeHUDWidget.h"

#include "Components/TextBlock.h"

void UBridgeHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Default to Helm so the shell reads correctly before any input.
	SetActiveStation(EStation::Helm);
}

void UBridgeHUDWidget::SetActiveStation(EStation Station)
{
	ApplyTab(HelmTab, Station == EStation::Helm);
	ApplyTab(WeaponsTab, Station == EStation::Weapons);
	ApplyTab(EngineeringTab, Station == EStation::Engineering);

	if (ActiveConsoleLabel)
	{
		FText Name;
		switch (Station)
		{
		case EStation::Weapons:     Name = FText::FromString(TEXT("WEAPONS"));     break;
		case EStation::Engineering: Name = FText::FromString(TEXT("ENGINEERING")); break;
		case EStation::Helm:
		default:                    Name = FText::FromString(TEXT("HELM"));        break;
		}
		ActiveConsoleLabel->SetText(Name);
	}
}

void UBridgeHUDWidget::ApplyTab(UTextBlock* Tab, bool bActive)
{
	if (Tab)
	{
		Tab->SetColorAndOpacity(FSlateColor(bActive ? ActiveColor : InactiveColor));
	}
}
