// SpaceGame — bridge simulator. HUD shell widget implementation.

#include "Core/BridgeHUDWidget.h"

#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Components/HealthComponent.h"
#include "Ships/Spaceship.h"

void UBridgeHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Default to Helm so the shell reads correctly before any input.
	SetActiveStation(EStation::Helm);
}

void UBridgeHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!HullText) { return; }

	const ASpaceship* Ship = Cast<ASpaceship>(GetOwningPlayerPawn());
	const UHealthComponent* Health = Ship ? Ship->GetHealthComp() : nullptr;
	if (!Health) { return; }

	const float MaxHull = FMath::Max(Health->GetMaxHull(), 1.f);
	const float Pct = Health->GetHull() / MaxHull;
	// Red alert (M29) rides the hull line: the whole readout goes red with a battle-stations flag.
	if (Ship->IsRedAlert())
	{
		HullText->SetText(FText::FromString(
			FString::Printf(TEXT("⚠ RED ALERT   HULL  %3d%%"), FMath::RoundToInt(Pct * 100.f))));
		HullText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.25f, 0.2f, 1.0f)));
		return;
	}
	HullText->SetText(FText::FromString(
		FString::Printf(TEXT("HULL  %3d%%"), FMath::RoundToInt(Pct * 100.f))));
	// Green when healthy, amber under 50%, red under 25%.
	const FLinearColor Color =
		Pct > 0.5f  ? FLinearColor(0.4f, 0.95f, 0.55f, 1.0f) :
		Pct > 0.25f ? FLinearColor(1.0f, 0.7f, 0.15f, 1.0f) :
		              FLinearColor(1.0f, 0.25f, 0.2f, 1.0f);
	HullText->SetColorAndOpacity(FSlateColor(Color));
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
	if (WeaponsConsole)
	{
		WeaponsConsole->SetVisibility(Station == EStation::Weapons
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
