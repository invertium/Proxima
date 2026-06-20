// SpaceGame — bridge simulator. Weapons console implementation.

#include "Core/WeaponsConsoleWidget.h"

#include "Components/WeaponComponent.h"
#include "Ships/Spaceship.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"
#include "GameFramework/Pawn.h"

UWeaponComponent* UWeaponsConsoleWidget::GetWeapon() const
{
	const ASpaceship* Ship = Cast<ASpaceship>(GetOwningPlayerPawn());
	return Ship ? Ship->GetWeaponComp() : nullptr;
}

void UWeaponsConsoleWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Wire the click controls to the same component calls the Right/Space keys use.
	if (CycleTargetBtn) { CycleTargetBtn->OnClicked.AddDynamic(this, &UWeaponsConsoleWidget::OnCycleTarget); }
	if (FireBtn)        { FireBtn->OnClicked.AddDynamic(this,        &UWeaponsConsoleWidget::OnFire); }
}

void UWeaponsConsoleWidget::OnCycleTarget()
{
	if (UWeaponComponent* Weapon = GetWeapon()) { Weapon->CycleTarget(); }
}

void UWeaponsConsoleWidget::OnFire()
{
	if (UWeaponComponent* Weapon = GetWeapon()) { Weapon->FireBeam(); }
}

void UWeaponsConsoleWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const ASpaceship* Ship = Cast<ASpaceship>(GetOwningPlayerPawn());
	const UWeaponComponent* Weapon = Ship ? Ship->GetWeaponComp() : nullptr;
	if (!Weapon)
	{
		return;
	}

	const AActor* Target = Weapon->GetCurrentTarget();
	if (TargetNameText)
	{
		TargetNameText->SetText(FText::FromString(
			Target ? FString::Printf(TEXT("TARGET  %s"), *Target->GetName()) : TEXT("TARGET  --")));
	}
	if (TargetRangeText)
	{
		const float Range = Weapon->GetTargetRange();
		TargetRangeText->SetText(FText::FromString(
			Range < 0.f ? TEXT("RANGE   --") : FString::Printf(TEXT("RANGE   %.0f"), Range)));
	}
	if (TargetStatusText)
	{
		const FString Status = !Target ? TEXT("NO TARGET")
			: (Weapon->IsTargetInRange() ? TEXT("IN RANGE") : TEXT("OUT OF RANGE"));
		TargetStatusText->SetText(FText::FromString(Status));
	}

	const float C = Weapon->GetCharge();
	const bool bReady = C >= 1.f;
	if (ChargeText)
	{
		ChargeText->SetText(FText::FromString(
			bReady ? TEXT("BEAM  READY") : FString::Printf(TEXT("BEAM  %3d%%"), FMath::RoundToInt(C * 100.f))));
	}
	if (ChargeBar)
	{
		ChargeBar->SetPercent(C);
		ChargeBar->SetFillColorAndOpacity(bReady ? ReadyColor : ChargingColor);
	}
}
