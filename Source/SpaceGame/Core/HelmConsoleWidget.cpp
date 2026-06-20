// SpaceGame — bridge simulator. Helm console implementation.

#include "Core/HelmConsoleWidget.h"

#include "Components/ShipMovementComponent.h"
#include "Ships/Spaceship.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "GameFramework/Pawn.h"

void UHelmConsoleWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const ASpaceship* Ship = Cast<ASpaceship>(GetOwningPlayerPawn());
	const UShipMovementComponent* Move = Ship ? Ship->GetMovementComp() : nullptr;
	if (!Move)
	{
		return;
	}

	const int32 ThrottlePct = FMath::RoundToInt(Move->GetThrottle() * 100.f);
	const float Speed = Move->GetSpeed();
	// Heading: 0..360, north (world +X / actor forward at spawn) = 0.
	const float Heading = FMath::Fmod(Ship->GetActorRotation().Yaw + 360.f, 360.f);

	if (ThrottleText)
	{
		ThrottleText->SetText(FText::FromString(FString::Printf(TEXT("THR  %3d%%"), ThrottlePct)));
	}
	if (SpeedText)
	{
		SpeedText->SetText(FText::FromString(FString::Printf(TEXT("SPD  %4.0f"), Speed)));
	}
	if (HeadingText)
	{
		HeadingText->SetText(FText::FromString(FString::Printf(TEXT("HDG  %03d"), FMath::RoundToInt(Heading))));
	}
	if (ThrottleBar)
	{
		ThrottleBar->SetPercent(Move->GetThrottle());
	}
}
