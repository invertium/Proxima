// SpaceGame — bridge simulator. Engineering console implementation.

#include "Core/EngineeringConsoleWidget.h"

#include "Core/BridgePlayerController.h"
#include "Components/DamageControlComponent.h"
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

	// Combat damage flags (M25): a damaged system's row goes amber with a DMG marker.
	const UDamageControlComponent* Dmg = Ship->GetDamageComp();
	const FLinearColor AmberColor(1.f, 0.65f, 0.1f, 1.f);

	auto Apply = [&](EShipSystem Sys, UTextBlock* Label, UProgressBar* Bar, const TCHAR* Name, bool bDamaged)
	{
		const float P = Power->GetSystemPower(Sys);
		const FLinearColor RowColor = bDamaged ? AmberColor : (Sys == Selected ? SelectedColor : IdleColor);
		if (Label)
		{
			Label->SetText(FText::FromString(
				FString::Printf(TEXT("%s  %3d%%%s"), Name, FMath::RoundToInt(P * 100.f),
					bDamaged ? TEXT("  ! DMG") : TEXT(""))));
			Label->SetColorAndOpacity(FSlateColor(RowColor));
		}
		if (Bar)
		{
			Bar->SetPercent(P / MaxPer);
			Bar->SetFillColorAndOpacity(RowColor);
		}
	};

	Apply(EShipSystem::Engine,  EngineText,  EngineBar,  TEXT("ENGINE "),
		Dmg && Dmg->IsDamaged(EDamageSystem::Engine));
	Apply(EShipSystem::Weapons, WeaponsText, WeaponsBar, TEXT("WEAPONS"),
		Dmg && Dmg->IsDamaged(EDamageSystem::Weapons));
	Apply(EShipSystem::Shields, ShieldsText, ShieldsBar, TEXT("SHIELDS"), false); // shields take no M25 damage

	if (ReactorText)
	{
		// Sensors have no power row, so their damage flag rides the reactor line.
		const bool bSensorsDown = Dmg && Dmg->IsDamaged(EDamageSystem::Sensors);
		ReactorText->SetText(FText::FromString(
			FString::Printf(TEXT("REACTOR LOAD  %3d%% / %3d%%%s"),
				FMath::RoundToInt(Power->GetTotalPower() * 100.f),
				FMath::RoundToInt(Power->ReactorBudget * 100.f),
				bSensorsDown ? TEXT("   ! SENSORS DMG") : TEXT(""))));
		ReactorText->SetColorAndOpacity(FSlateColor(bSensorsDown ? AmberColor : FLinearColor::White));
	}
}
