// SpaceGame — bridge simulator. HUD shell widget implementation.

#include "Core/BridgeHUDWidget.h"

#include "Components/TextBlock.h"
#include "Components/Widget.h"

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

	// Each console is visible only while its station is active (Weapons arrives M8).
	if (HelmConsole)
	{
		HelmConsole->SetVisibility(Station == EStation::Helm
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}
	if (EngineeringConsole)
	{
		EngineeringConsole->SetVisibility(Station == EStation::Engineering
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}

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
