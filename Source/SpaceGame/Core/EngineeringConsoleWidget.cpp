// SpaceGame — bridge simulator. Engineering console implementation.

#include "Core/EngineeringConsoleWidget.h"

#include "Core/BridgePlayerController.h"
#include "Components/PowerComponent.h"
#include "Ships/Spaceship.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "GameFramework/Pawn.h"

void UEngineeringConsoleWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const ASpaceship* Ship = Cast<ASpaceship>(GetOwningPlayerPawn());
	const UPowerComponent* Power = Ship ? Ship->GetPowerComp() : nullptr;
	if (!Power)
	{
		return;
	}

	const ABridgePlayerController* PC = Cast<ABridgePlayerController>(GetOwningPlayer());
	const EShipSystem Selected = PC ? PC->GetSelectedSystem() : EShipSystem::Engine;
	const float MaxPer = FMath::Max(Power->MaxPerSystem, 0.01f);

	auto Apply = [&](EShipSystem Sys, UTextBlock* Label, UProgressBar* Bar, const TCHAR* Name)
	{
		const float P = Power->GetSystemPower(Sys);
		if (Label)
		{
			Label->SetText(FText::FromString(
				FString::Printf(TEXT("%s  %3d%%"), Name, FMath::RoundToInt(P * 100.f))));
			Label->SetColorAndOpacity(FSlateColor(Sys == Selected ? SelectedColor : IdleColor));
		}
		if (Bar)
		{
			Bar->SetPercent(P / MaxPer);
			Bar->SetFillColorAndOpacity(Sys == Selected ? SelectedColor : IdleColor);
		}
	};

	Apply(EShipSystem::Engine,  EngineText,  EngineBar,  TEXT("ENGINE "));
	Apply(EShipSystem::Weapons, WeaponsText, WeaponsBar, TEXT("WEAPONS"));
	Apply(EShipSystem::Shields, ShieldsText, ShieldsBar, TEXT("SHIELDS"));

	if (ReactorText)
	{
		ReactorText->SetText(FText::FromString(
			FString::Printf(TEXT("REACTOR LOAD  %3d%% / %3d%%"),
				FMath::RoundToInt(Power->GetTotalPower() * 100.f),
				FMath::RoundToInt(Power->ReactorBudget * 100.f))));
	}
}
