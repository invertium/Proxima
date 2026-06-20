// SpaceGame — bridge simulator. Beam weapon implementation.

#include "Components/WeaponComponent.h"

#include "Components/PowerComponent.h"
#include "Components/RadarContactComponent.h"
#include "Components/HealthComponent.h"
#include "GameFramework/Actor.h"
#include "DrawDebugHelpers.h"
#include "UObject/UObjectIterator.h"

UWeaponComponent::UWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

float UWeaponComponent::WeaponPowerScale() const
{
	UWeaponComponent* MutableThis = const_cast<UWeaponComponent*>(this);
	if (!MutableThis->CachedPower)
	{
		if (const AActor* Owner = GetOwner())
		{
			MutableThis->CachedPower = Owner->FindComponentByClass<UPowerComponent>();
		}
	}
	return CachedPower ? CachedPower->GetSystemPower(EShipSystem::Weapons) : 1.0f;
}

float UWeaponComponent::GetTargetRange() const
{
	if (!CurrentTarget)
	{
		return -1.f;
	}
	const AActor* Owner = GetOwner();
	return Owner ? FVector::Dist(Owner->GetActorLocation(), CurrentTarget->GetActorLocation()) : -1.f;
}

bool UWeaponComponent::IsTargetInRange() const
{
	const float Range = GetTargetRange();
	return Range >= 0.f && Range <= BeamRange;
}

void UWeaponComponent::CycleTarget()
{
	const UWorld* World = GetWorld();
	const AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return;
	}

	// Gather radar-contact actors in this world (excluding the player ship), stable order.
	TArray<AActor*> Contacts;
	for (TObjectIterator<URadarContactComponent> It; It; ++It)
	{
		const URadarContactComponent* C = *It;
		if (!IsValid(C) || C->GetWorld() != World)
		{
			continue;
		}
		AActor* A = C->GetOwner();
		if (A && A != Owner)
		{
			Contacts.AddUnique(A);
		}
	}
	Contacts.Sort([](const AActor& A, const AActor& B) { return A.GetName() < B.GetName(); });

	if (Contacts.Num() == 0)
	{
		CurrentTarget = nullptr;
		UE_LOG(LogTemp, Log, TEXT("[Weapon] Cycle target -> none in range/world"));
		return;
	}

	const int32 Current = CurrentTarget ? Contacts.IndexOfByKey(CurrentTarget) : INDEX_NONE;
	const int32 Next = (Current + 1) % Contacts.Num();
	CurrentTarget = Contacts[Next];
	UE_LOG(LogTemp, Log, TEXT("[Weapon] Target -> %s (range %.0f uu)"),
		*CurrentTarget->GetName(), GetTargetRange());
}

bool UWeaponComponent::FireBeam()
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}
	if (Charge < 1.f)
	{
		UE_LOG(LogTemp, Log, TEXT("[Weapon] Fire blocked — charge %.0f%%"), Charge * 100.f);
		return false;
	}
	if (!CurrentTarget)
	{
		UE_LOG(LogTemp, Log, TEXT("[Weapon] Fire blocked — no target"));
		return false;
	}
	if (!IsTargetInRange())
	{
		UE_LOG(LogTemp, Log, TEXT("[Weapon] Fire blocked — target out of range (%.0f > %.0f)"),
			GetTargetRange(), BeamRange);
		return false;
	}

	// Fire: empty the charge, arm the beam visual, and land damage on the target.
	Charge = 0.f;
	BeamStart = Owner->GetActorLocation();
	BeamEnd = CurrentTarget->GetActorLocation();
	BeamDrawTimer = BeamDrawTime;

	UE_LOG(LogTemp, Log, TEXT("[Weapon] BEAM FIRED at %s (range %.0f uu)"),
		*CurrentTarget->GetName(), GetTargetRange());

	// Apply damage: shields absorb first, then hull (UHealthComponent handles death).
	if (UHealthComponent* Health = CurrentTarget->FindComponentByClass<UHealthComponent>())
	{
		Health->ApplyDamage(BeamDamage);
	}
	return true;
}

void UWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Recharge, scaled by Weapons power (0 power → never charges).
	Charge = FMath::Clamp(Charge + BaseRechargeRate * WeaponPowerScale() * DeltaTime, 0.f, 1.f);

	// Keep the fired beam drawn for BeamDrawTime (one-frame lifetime, redrawn each tick):
	// a bright beam line plus an impact flare sphere at the hit point.
	if (BeamDrawTimer > 0.f)
	{
		if (const UWorld* World = GetWorld())
		{
			DrawDebugLine(World, BeamStart, BeamEnd, FColor(80, 200, 255), false, -1.f, 0, 14.f);
			DrawDebugSphere(World, BeamEnd, 140.f, 12, FColor(160, 230, 255), false, -1.f, 0, 5.f);
		}
		BeamDrawTimer -= DeltaTime;
	}
}
