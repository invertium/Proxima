// SpaceGame — bridge simulator. Science sensors implementation.

#include "Components/ScienceComponent.h"

#include "Components/DamageControlComponent.h"
#include "Components/HealthComponent.h"
#include "Components/RadarContactComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectIterator.h"

UScienceComponent::UScienceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UScienceComponent::CycleTarget()
{
	const UWorld* World = GetWorld();
	const AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return;
	}

	// Gather radar-contact actors in this world (excluding our own ship), stable order.
	TArray<AActor*> Contacts;
	for (TObjectIterator<URadarContactComponent> It; It; ++It)
	{
		const URadarContactComponent* C = *It;
		if (!IsValid(C) || C->GetWorld() != World || !C->bTargetable)
		{
			continue; // skip friendly/non-targetable contacts (e.g. the dock station)
		}
		AActor* A = C->GetOwner();
		if (A && A != Owner)
		{
			Contacts.AddUnique(A);
		}
	}
	Contacts.Sort([](const AActor& A, const AActor& B) { return A.GetName() < B.GetName(); });

	// Any target change clears the scan — a new contact must be scanned fresh.
	ScanProgress = 0.f;
	bScanning = false;
	bScanned = false;

	if (Contacts.Num() == 0)
	{
		ScanTarget = nullptr;
		return;
	}

	const int32 Current = ScanTarget ? Contacts.IndexOfByKey(ScanTarget) : INDEX_NONE;
	const int32 Next = (Current + 1) % Contacts.Num();
	ScanTarget = Contacts[Next];
	UE_LOG(LogTemp, Log, TEXT("[Science] Scan target -> %s"), *ScanTarget->GetName());
}

float UScienceComponent::GetEffectiveScanRange() const
{
	float Mult = 1.f;
	if (const AActor* Owner = GetOwner())
	{
		if (const UDamageControlComponent* Dmg = Owner->FindComponentByClass<UDamageControlComponent>())
		{
			Mult = Dmg->GetMultiplier(EDamageSystem::Sensors);
		}
	}
	return ScanRange * Mult;
}

bool UScienceComponent::IsTargetInScanRange() const
{
	if (!IsValid(ScanTarget) || !GetOwner())
	{
		return true; // nothing selected — range isn't the blocker
	}
	return FVector::Dist(ScanTarget->GetActorLocation(), GetOwner()->GetActorLocation())
		<= GetEffectiveScanRange();
}

void UScienceComponent::BeginScan()
{
	if (!ScanTarget || bScanned)
	{
		return;
	}
	if (!IsTargetInScanRange())
	{
		UE_LOG(LogTemp, Log, TEXT("[Science] %s is beyond sensor range (%.0f uu) — close the distance"),
			*ScanTarget->GetName(), GetEffectiveScanRange());
		return;
	}
	bScanning = true;
	UE_LOG(LogTemp, Log, TEXT("[Science] Scanning %s..."), *ScanTarget->GetName());
}

void UScienceComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Lost target (destroyed) — drop the scan.
	if (!IsValid(ScanTarget))
	{
		ScanTarget = nullptr;
		ScanProgress = 0.f;
		bScanning = false;
		bScanned = false;
		return;
	}

	if (bScanning && !bScanned)
	{
		const float Step = ScanDuration > 0.f ? DeltaTime / ScanDuration : 1.f;
		ScanProgress = FMath::Clamp(ScanProgress + Step, 0.f, 1.f);
		if (ScanProgress >= 1.f)
		{
			bScanning = false;
			bScanned = true;
			UE_LOG(LogTemp, Log, TEXT("[Science] Scan complete: %s"), *ScanTarget->GetName());
		}
	}
}

UHealthComponent* UScienceComponent::TargetHealth() const
{
	if (!bScanned || !IsValid(ScanTarget))
	{
		return nullptr;
	}
	return ScanTarget->FindComponentByClass<UHealthComponent>();
}

float UScienceComponent::GetTargetHull() const
{
	const UHealthComponent* H = TargetHealth();
	return H ? H->GetHull() : -1.f;
}

float UScienceComponent::GetTargetMaxHull() const
{
	const UHealthComponent* H = TargetHealth();
	return H ? H->GetMaxHull() : -1.f;
}

float UScienceComponent::GetTargetShield() const
{
	const UHealthComponent* H = TargetHealth();
	return H ? H->GetShield() : -1.f;
}

float UScienceComponent::GetTargetMaxShield() const
{
	const UHealthComponent* H = TargetHealth();
	return H ? H->GetMaxShield() : -1.f;
}
