// SpaceGame — bridge simulator. Subsystem damage implementation (M25).

#include "Components/DamageControlComponent.h"

#include "Components/HealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

UDamageControlComponent::UDamageControlComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	Damaged.Init(false, static_cast<int32>(EDamageSystem::Count));

	static ConstructorHelpers::FObjectFinder<USoundBase> Alarm(TEXT("/Game/Audio/S_Alarm.S_Alarm"));
	if (Alarm.Succeeded()) { AlarmSound = Alarm.Object; }
}

void UDamageControlComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const AActor* Owner = GetOwner())
	{
		if (UHealthComponent* Health = Owner->FindComponentByClass<UHealthComponent>())
		{
			Health->OnDamaged.AddUniqueDynamic(this, &UDamageControlComponent::HandleOwnerDamaged);
		}
	}
}

const TCHAR* UDamageControlComponent::SystemName(EDamageSystem System)
{
	switch (System)
	{
	case EDamageSystem::Engine:  return TEXT("ENGINE");
	case EDamageSystem::Weapons: return TEXT("WEAPONS");
	case EDamageSystem::Sensors: return TEXT("SENSORS");
	default:                     return TEXT("NONE");
	}
}

bool UDamageControlComponent::IsDamaged(EDamageSystem System) const
{
	const int32 Idx = static_cast<int32>(System);
	return Damaged.IsValidIndex(Idx) && Damaged[Idx];
}

int32 UDamageControlComponent::GetDamagedCount() const
{
	int32 Count = 0;
	for (const bool b : Damaged) { if (b) { ++Count; } }
	return Count;
}

void UDamageControlComponent::DamageSystem(EDamageSystem System)
{
	const int32 Idx = static_cast<int32>(System);
	if (!Damaged.IsValidIndex(Idx) || Damaged[Idx])
	{
		return;
	}
	Damaged[Idx] = true;
	UE_LOG(LogTemp, Log, TEXT("[DamageCtl] %s DAMAGED — running at %d%%"),
		SystemName(System), FMath::RoundToInt(DamagedMultiplier * 100.f));

	// Only beep when the ship is already in the red. Routine system knockouts at healthy hull
	// flag amber silently — the low-hull klaxon owns the continuous critical alarm, so this
	// punctuates it rather than beeping on every scratch.
	if (AlarmSound && IsHullCritical())
	{
		UGameplayStatics::PlaySound2D(this, AlarmSound, 0.6f);
	}
}

bool UDamageControlComponent::IsHullCritical() const
{
	const AActor* Owner = GetOwner();
	if (!Owner) { return false; }
	const UHealthComponent* Health = Owner->FindComponentByClass<UHealthComponent>();
	if (!Health) { return false; }
	const float MaxHull = Health->GetMaxHull();
	const float Frac = MaxHull > 0.f ? Health->GetHull() / MaxHull : 0.f;
	return Frac <= AlarmHullFraction;
}

void UDamageControlComponent::RepairSystem(EDamageSystem System)
{
	const int32 Idx = static_cast<int32>(System);
	if (!Damaged.IsValidIndex(Idx) || !Damaged[Idx])
	{
		return;
	}
	Damaged[Idx] = false;
	UE_LOG(LogTemp, Log, TEXT("[DamageCtl] %s repaired — back to full output"), SystemName(System));
}

void UDamageControlComponent::RepairAll()
{
	for (int32 i = 0; i < Damaged.Num(); ++i)
	{
		if (Damaged[i]) { RepairSystem(static_cast<EDamageSystem>(i)); }
	}
	RepairWelds = 0;
}

EDamageSystem UDamageControlComponent::GetRepairTarget() const
{
	for (int32 i = 0; i < Damaged.Num(); ++i)
	{
		if (Damaged[i]) { return static_cast<EDamageSystem>(i); }
	}
	return EDamageSystem::Count;
}

bool UDamageControlComponent::CreditRepairWeld()
{
	const EDamageSystem Target = GetRepairTarget();
	if (Target == EDamageSystem::Count)
	{
		return false; // nothing damaged — the weld may restore hull instead
	}
	if (++RepairWelds >= WeldsPerSystemRepair)
	{
		RepairWelds = 0;
		RepairSystem(Target);
	}
	return true;
}

void UDamageControlComponent::HandleOwnerDamaged(float EffectiveDamage, float HullDamage, float HullRemaining)
{
	if (HullRemaining <= 0.f)
	{
		return; // dead — the defeat flow owns what happens next
	}
	// Only hits that actually reach hull threaten systems. Using the real hull-damage amount (not
	// "is any shield left?") means an exactly-shield-depleting hit no longer knocks out a subsystem,
	// while a shield-bypassing torpedo correctly can (audit BUG-06).
	if (HullDamage <= 0.f)
	{
		return;
	}
	if (FMath::FRand() >= DamageChance)
	{
		return;
	}

	// Pick a random still-working system; all three down = nothing left to break.
	TArray<EDamageSystem> Working;
	for (int32 i = 0; i < Damaged.Num(); ++i)
	{
		if (!Damaged[i]) { Working.Add(static_cast<EDamageSystem>(i)); }
	}
	if (!Working.IsEmpty())
	{
		DamageSystem(Working[FMath::RandRange(0, Working.Num() - 1)]);
	}
}
