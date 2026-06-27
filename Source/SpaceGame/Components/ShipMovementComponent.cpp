// SpaceGame — bridge simulator. Ship impulse movement implementation.

#include "Components/ShipMovementComponent.h"

#include "Components/PowerComponent.h"
#include "GameFramework/Actor.h"

UShipMovementComponent::UShipMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

float UShipMovementComponent::EnginePowerScale() const
{
	UShipMovementComponent* MutableThis = const_cast<UShipMovementComponent*>(this);
	if (!MutableThis->CachedPower)
	{
		if (const AActor* Owner = GetOwner())
		{
			MutableThis->CachedPower = Owner->FindComponentByClass<UPowerComponent>();
		}
	}
	return CachedPower ? CachedPower->GetSystemPower(EShipSystem::Engine) : 1.0f;
}

float UShipMovementComponent::GetEffectiveMaxSpeed() const
{
	return MaxSpeed * EnginePowerScale();
}

void UShipMovementComponent::SetThrottle(float InThrottle)
{
	if (bInputLocked) { return; }
	ThrottleInput = FMath::Clamp(InThrottle, 0.f, 1.f);
	UE_LOG(LogTemp, Log, TEXT("[ShipMovement] Throttle set to %.2f (target speed %.0f uu/s)"),
		ThrottleInput, ThrottleInput * MaxSpeed);
}

void UShipMovementComponent::SetTurn(float InTurn)
{
	if (bInputLocked) { return; }
	TurnInput = FMath::Clamp(InTurn, -1.f, 1.f);
	UE_LOG(LogTemp, Log, TEXT("[ShipMovement] Turn set to %.2f"), TurnInput);
}

void UShipMovementComponent::SetInputLocked(bool bLocked)
{
	bInputLocked = bLocked;
	if (bLocked)
	{
		// Cut helm input so the ship coasts to a stop (CurrentSpeed eases down in Tick).
		ThrottleInput = 0.f;
		TurnInput = 0.f;
	}
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
	// Engine power (Engineering, M7) scales the achievable max speed.
	const float TargetSpeed = ThrottleInput * GetEffectiveMaxSpeed();
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
