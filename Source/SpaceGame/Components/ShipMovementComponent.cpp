// SpaceGame — bridge simulator. Ship impulse movement implementation.

#include "Components/ShipMovementComponent.h"

#include "GameFramework/Actor.h"

UShipMovementComponent::UShipMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UShipMovementComponent::SetThrottle(float InThrottle)
{
	ThrottleInput = FMath::Clamp(InThrottle, 0.f, 1.f);
	UE_LOG(LogTemp, Log, TEXT("[ShipMovement] Throttle set to %.2f (target speed %.0f uu/s)"),
		ThrottleInput, ThrottleInput * MaxSpeed);
}

void UShipMovementComponent::SetTurn(float InTurn)
{
	TurnInput = FMath::Clamp(InTurn, -1.f, 1.f);
	UE_LOG(LogTemp, Log, TEXT("[ShipMovement] Turn set to %.2f"), TurnInput);
}

void UShipMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Impulse feel: throttle sets a target speed the ship eases toward.
	const float TargetSpeed = ThrottleInput * MaxSpeed;
	CurrentSpeed = FMath::FInterpConstantTo(CurrentSpeed, TargetSpeed, DeltaTime, Acceleration);

	// Yaw turn.
	if (!FMath::IsNearlyZero(TurnInput))
	{
		const float DeltaYaw = TurnInput * MaxTurnRate * DeltaTime;
		Owner->AddActorWorldRotation(FRotator(0.f, DeltaYaw, 0.f));
	}

	// Forward translation along the owner's +X.
	if (!FMath::IsNearlyZero(CurrentSpeed))
	{
		const FVector Delta = Owner->GetActorForwardVector() * CurrentSpeed * DeltaTime;
		Owner->AddActorWorldOffset(Delta, /*bSweep=*/true);
	}
}
