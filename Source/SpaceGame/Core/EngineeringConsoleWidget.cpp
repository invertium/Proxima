// SpaceGame — bridge simulator. Engineering console implementation.

#include "Core/EngineeringConsoleWidget.h"

#include "Core/BridgePlayerController.h"
#include "Components/PowerComponent.h"
#include "Ships/Spaceship.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"
#include "GameFramework/Pawn.h"

void UEngineeringConsoleWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Wire the +/- buttons. Bound here (not in the WBP graph) so all logic stays in C++.
	if (EngineMinusBtn)  { EngineMinusBtn->OnClicked.AddDynamic(this,  &UEngineeringConsoleWidget::OnEngineMinus); }
	if (EnginePlusBtn)   { EnginePlusBtn->OnClicked.AddDynamic(this,   &UEngineeringConsoleWidget::OnEnginePlus); }
	if (WeaponsMinusBtn) { WeaponsMinusBtn->OnClicked.AddDynamic(this, &UEngineeringConsoleWidget::OnWeaponsMinus); }
	if (WeaponsPlusBtn)  { WeaponsPlusBtn->OnClicked.AddDynamic(this,  &UEngineeringConsoleWidget::OnWeaponsPlus); }
	if (ShieldsMinusBtn) { ShieldsMinusBtn->OnClicked.AddDynamic(this, &UEngineeringConsoleWidget::OnShieldsMinus); }
	if (ShieldsPlusBtn)  { ShieldsPlusBtn->OnClicked.AddDynamic(this,  &UEngineeringConsoleWidget::OnShieldsPlus); }
}

void UEngineeringConsoleWidget::OnEngineMinus()  { if (ABridgePlayerController* PC = Cast<ABridgePlayerController>(GetOwningPlayer())) { PC->EngAdjustSystem(EShipSystem::Engine,  false); } }
void UEngineeringConsoleWidget::OnEnginePlus()   { if (ABridgePlayerController* PC = Cast<ABridgePlayerController>(GetOwningPlayer())) { PC->EngAdjustSystem(EShipSystem::Engine,  true); } }
void UEngineeringConsoleWidget::OnWeaponsMinus() { if (ABridgePlayerController* PC = Cast<ABridgePlayerController>(GetOwningPlayer())) { PC->EngAdjustSystem(EShipSystem::Weapons, false); } }
void UEngineeringConsoleWidget::OnWeaponsPlus()  { if (ABridgePlayerController* PC = Cast<ABridgePlayerController>(GetOwningPlayer())) { PC->EngAdjustSystem(EShipSystem::Weapons, true); } }
void UEngineeringConsoleWidget::OnShieldsMinus() { if (ABridgePlayerController* PC = Cast<ABridgePlayerController>(GetOwningPlayer())) { PC->EngAdjustSystem(EShipSystem::Shields, false); } }
void UEngineeringConsoleWidget::OnShieldsPlus()  { if (ABridgePlayerController* PC = Cast<ABridgePlayerController>(GetOwningPlayer())) { PC->EngAdjustSystem(EShipSystem::Shields, true); } }

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
