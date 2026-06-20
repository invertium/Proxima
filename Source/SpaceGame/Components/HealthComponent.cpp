// SpaceGame — bridge simulator. Hull/shield health implementation.

#include "Components/HealthComponent.h"

#include "GameFramework/Actor.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	// Start full; designers tune Max* in defaults, runtime pools mirror them at spawn.
	Hull = MaxHull;
	Shield = MaxShield;
}

float UHealthComponent::ApplyDamage(float Amount)
{
	if (Amount <= 0.f || Hull <= 0.f)
	{
		return Hull;
	}

	// Shields absorb first, the overflow bleeds into hull.
	const float ToShield = FMath::Min(Shield, Amount);
	Shield -= ToShield;
	const float ToHull = Amount - ToShield;
	Hull = FMath::Max(0.f, Hull - ToHull);

	const AActor* Owner = GetOwner();
	UE_LOG(LogTemp, Log, TEXT("[Health] %s took %.0f dmg -> shield %.0f/%.0f hull %.0f/%.0f"),
		Owner ? *Owner->GetName() : TEXT("?"), Amount, Shield, MaxShield, Hull, MaxHull);

	if (Hull <= 0.f && !bDeathBroadcast)
	{
		bDeathBroadcast = true;
		UE_LOG(LogTemp, Log, TEXT("[Health] %s destroyed"), Owner ? *Owner->GetName() : TEXT("?"));
		OnDeath.Broadcast(GetOwner());
	}

	return Hull;
}
