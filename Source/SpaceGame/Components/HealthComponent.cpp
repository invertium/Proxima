// SpaceGame — bridge simulator. Hull/shield health implementation.

#include "Components/HealthComponent.h"

#include "Components/PowerComponent.h"
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

float UHealthComponent::GetShieldMitigation() const
{
	if (!CachedPower)
	{
		if (const AActor* Owner = GetOwner())
		{
			CachedPower = Owner->FindComponentByClass<UPowerComponent>();
		}
	}
	if (!CachedPower)
	{
		return 0.f;
	}
	const float ShieldPower = CachedPower->GetSystemPower(EShipSystem::Shields);
	return FMath::Clamp(ShieldPower * ShieldMitigationScale, 0.f, MaxMitigation);
}

float UHealthComponent::ApplyDamage(float Amount, bool bBypassShield)
{
	if (Amount <= 0.f || Hull <= 0.f)
	{
		return Hull;
	}

	float Effective;
	float Mitigation = 0.f;
	if (bBypassShield)
	{
		// Torpedo strike: ignore shield-power mitigation and the shield pool entirely —
		// the full payload lands on hull (M17 torpedoes).
		Effective = Amount;
		Hull = FMath::Max(0.f, Hull - Effective);
	}
	else
	{
		// Shield power mitigates incoming damage (D11): more Shields power → softer hits.
		Mitigation = GetShieldMitigation();
		Effective = Amount * (1.f - Mitigation);

		// Shields pool absorbs first, the overflow bleeds into hull.
		const float ToShield = FMath::Min(Shield, Effective);
		Shield -= ToShield;
		const float ToHull = Effective - ToShield;
		Hull = FMath::Max(0.f, Hull - ToHull);
	}

	const AActor* Owner = GetOwner();
	UE_LOG(LogTemp, Log, TEXT("[Health] %s took %.1f dmg%s (-%.0f%% shield, %.1f effective) -> shield %.0f/%.0f hull %.0f/%.0f"),
		Owner ? *Owner->GetName() : TEXT("?"), Amount, bBypassShield ? TEXT(" [TORPEDO]") : TEXT(""),
		Mitigation * 100.f, Effective, Shield, MaxShield, Hull, MaxHull);

	// Per-hit feedback hook (camera shake, hit FX). Effective>0 here by construction.
	OnDamaged.Broadcast(Effective, Hull);

	if (Hull <= 0.f && !bDeathBroadcast)
	{
		bDeathBroadcast = true;
		UE_LOG(LogTemp, Log, TEXT("[Health] %s destroyed"), Owner ? *Owner->GetName() : TEXT("?"));
		OnDeath.Broadcast(GetOwner());
	}

	return Hull;
}

float UHealthComponent::RepairHull(float Amount)
{
	// Engineering repairs (M17). Caps at MaxHull; a dead hull (0) stays dead — no reviving.
	if (Amount <= 0.f || Hull <= 0.f)
	{
		return Hull;
	}
	Hull = FMath::Min(MaxHull, Hull + Amount);

	const AActor* Owner = GetOwner();
	UE_LOG(LogTemp, Log, TEXT("[Health] %s repaired +%.1f -> hull %.0f/%.0f"),
		Owner ? *Owner->GetName() : TEXT("?"), Amount, Hull, MaxHull);
	return Hull;
}

void UHealthComponent::ResetPools()
{
	Hull = MaxHull;
	Shield = MaxShield;
	bDeathBroadcast = false;
}
