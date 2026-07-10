// SpaceGame — bridge simulator. Beam weapon implementation.

#include "Components/WeaponComponent.h"

#include "Components/DamageControlComponent.h"
#include "Components/PowerComponent.h"
#include "Components/RadarContactComponent.h"
#include "Components/HealthComponent.h"
#include "FX/BeamFx.h"
#include "FX/ExplosionFx.h"
#include "Ships/EnemyShip.h"
#include "Ships/Spaceship.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectIterator.h"

UWeaponComponent::UWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(
		TEXT("/Game/Art/Materials/M_GlowCyan.M_GlowCyan"));
	if (Glow.Succeeded()) { BeamMaterial = Glow.Object; }

	static ConstructorHelpers::FObjectFinder<USoundBase> Fire(
		TEXT("/Game/Audio/S_BeamFire.S_BeamFire"));
	if (Fire.Succeeded()) { FireSound = Fire.Object; }
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

float UWeaponComponent::WeaponDamageScale() const
{
	UWeaponComponent* MutableThis = const_cast<UWeaponComponent*>(this);
	if (!MutableThis->CachedDamage)
	{
		if (const AActor* Owner = GetOwner())
		{
			MutableThis->CachedDamage = Owner->FindComponentByClass<UDamageControlComponent>();
		}
	}
	return CachedDamage ? CachedDamage->GetMultiplier(EDamageSystem::Weapons) : 1.0f;
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

bool UWeaponComponent::IsTargetInArc() const
{
	return IsTargetWithinArc(FireArcDeg);
}

bool UWeaponComponent::IsTargetWithinArc(float ArcDeg) const
{
	const AActor* Owner = GetOwner();
	if (!Owner || !CurrentTarget)
	{
		return false;
	}

	// Yaw-plane check (combat is planar): angle between the bow and the line to the target.
	FVector Forward = Owner->GetActorForwardVector();
	FVector ToTarget = CurrentTarget->GetActorLocation() - Owner->GetActorLocation();
	Forward.Z = 0.f;
	ToTarget.Z = 0.f;
	if (!Forward.Normalize() || !ToTarget.Normalize())
	{
		return false;
	}

	const float CosHalfArc = FMath::Cos(FMath::DegreesToRadians(ArcDeg * 0.5f));
	return FVector::DotProduct(Forward, ToTarget) >= CosHalfArc;
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
	// Docked = combat-safe both ways: the ship is invulnerable at the station, so its weapons
	// are offline too — no sniping enemies from inside the dock's protection.
	if (const ASpaceship* OwnerShip = Cast<ASpaceship>(Owner))
	{
		if (OwnerShip->IsDocked())
		{
			UE_LOG(LogTemp, Log, TEXT("[Weapon] Fire blocked — weapons offline while docked"));
			return false;
		}
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
	if (!IsTargetInArc())
	{
		UE_LOG(LogTemp, Log, TEXT("[Weapon] Fire blocked — target outside firing arc (turn to face it)"));
		return false;
	}

	// Fire: empty the charge, spawn the beam FX, and land damage on the target.
	Charge = 0.f;
	ABeamFx::Spawn(GetWorld(), Owner->GetActorLocation(), CurrentTarget->GetActorLocation(),
		BeamMaterial, 22.f, BeamDrawTime);

	// Recoil kick on the firing ship's camera + fire SFX (M14 game-feel).
	if (ASpaceship* Ship = Cast<ASpaceship>(GetOwner()))
	{
		Ship->AddCameraTrauma(FireTrauma);
	}
	if (FireSound)
	{
		// Per-shot pitch jitter so sustained beam fire stays lively (M14).
		UGameplayStatics::PlaySound2D(this, FireSound, 1.f, FMath::FRandRange(0.94f, 1.06f));
	}

	UE_LOG(LogTemp, Log, TEXT("[Weapon] BEAM FIRED at %s (range %.0f uu)"),
		*CurrentTarget->GetName(), GetTargetRange());

	// Apply damage: shields absorb first, then hull (UHealthComponent handles death). M26: an
	// armored hull (cruiser) mitigates beams until Science scans it for the weakpoint.
	float Damage = BeamDamage;
	if (const AEnemyShip* Enemy = Cast<AEnemyShip>(CurrentTarget))
	{
		const float ArmorMult = Enemy->GetBeamDamageMultiplier();
		if (ArmorMult < 1.f)
		{
			Damage *= ArmorMult;
			UE_LOG(LogTemp, Log, TEXT("[Weapon] %s's armor mitigates the beam (%.0f%% damage) — scan it to expose the weakpoint"),
				*Enemy->GetCallsign(), ArmorMult * 100.f);
		}
	}
	if (UHealthComponent* Health = CurrentTarget->FindComponentByClass<UHealthComponent>())
	{
		Health->ApplyDamage(Damage);
	}

	// Small cyan impact flash where the beam lands (no boom — bPlaySound off).
	AExplosionFx::Spawn(GetWorld(), CurrentTarget->GetActorLocation(), 260.f, BeamMaterial, 0.22f, false);
	return true;
}

void UWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// A destroyed contact is no longer a lock — drop it so the consoles stop showing a dead
	// target and the next cycle starts fresh (M24).
	if (CurrentTarget && !IsValid(CurrentTarget))
	{
		CurrentTarget = nullptr;
		UE_LOG(LogTemp, Log, TEXT("[Weapon] Target destroyed — lock cleared"));
	}

	// Recharge, scaled by Weapons power (0 power → never charges) and combat damage (M25:
	// a damaged weapons system recharges at half rate until Engineering repairs it).
	Charge = FMath::Clamp(Charge + BaseRechargeRate * WeaponPowerScale() * WeaponDamageScale() * DeltaTime, 0.f, 1.f);
}
