// SpaceGame — bridge simulator. Torpedo launcher implementation.

#include "Components/TorpedoLauncherComponent.h"

#include "Components/WeaponComponent.h"
#include "FX/TorpedoProjectile.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Ships/Spaceship.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

UTorpedoLauncherComponent::UTorpedoLauncherComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(
		TEXT("/Game/Art/Materials/M_GlowOrange.M_GlowOrange"));
	if (Glow.Succeeded()) { TorpedoMaterial = Glow.Object; }

	static ConstructorHelpers::FObjectFinder<USoundBase> Fire(
		TEXT("/Game/Audio/S_BeamFire.S_BeamFire"));
	if (Fire.Succeeded()) { FireSound = Fire.Object; }
}

void UTorpedoLauncherComponent::BeginPlay()
{
	Super::BeginPlay();
	Ammo = MaxAmmo;
	ReloadTimer = 0.f;
}

UWeaponComponent* UTorpedoLauncherComponent::GetWeapon() const
{
	UTorpedoLauncherComponent* MutableThis = const_cast<UTorpedoLauncherComponent*>(this);
	if (!MutableThis->CachedWeapon)
	{
		if (const AActor* Owner = GetOwner())
		{
			MutableThis->CachedWeapon = Owner->FindComponentByClass<UWeaponComponent>();
		}
	}
	return CachedWeapon;
}

bool UTorpedoLauncherComponent::IsReady() const
{
	const UWeaponComponent* Weapon = GetWeapon();
	return Ammo > 0 && ReloadTimer <= 0.f && Weapon
		&& Weapon->GetCurrentTarget() != nullptr && Weapon->IsTargetInArc();
}

float UTorpedoLauncherComponent::GetReloadFraction() const
{
	if (ReloadTime <= 0.f) { return 1.f; }
	return FMath::Clamp(1.f - (ReloadTimer / ReloadTime), 0.f, 1.f);
}

bool UTorpedoLauncherComponent::Fire()
{
	const AActor* Owner = GetOwner();
	if (!Owner) { return false; }

	if (Ammo <= 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[Torpedo] Fire blocked — magazine empty"));
		return false;
	}
	if (ReloadTimer > 0.f)
	{
		UE_LOG(LogTemp, Log, TEXT("[Torpedo] Fire blocked — reloading (%.1fs)"), ReloadTimer);
		return false;
	}

	UWeaponComponent* Weapon = GetWeapon();
	AActor* TargetActor = Weapon ? Weapon->GetCurrentTarget() : nullptr;
	if (!TargetActor)
	{
		UE_LOG(LogTemp, Log, TEXT("[Torpedo] Fire blocked — no target"));
		return false;
	}
	if (!Weapon->IsTargetInArc())
	{
		UE_LOG(LogTemp, Log, TEXT("[Torpedo] Fire blocked — target outside firing arc (turn to face it)"));
		return false;
	}

	// Spend a round, start the reload, and launch the homing projectile.
	--Ammo;
	ReloadTimer = ReloadTime;
	ATorpedoProjectile::Spawn(GetWorld(), Owner->GetActorLocation(), TargetActor,
		TorpedoMaterial, TorpedoDamage, TorpedoSpeed);

	if (ASpaceship* Ship = Cast<ASpaceship>(GetOwner()))
	{
		Ship->AddCameraTrauma(FireTrauma);
	}
	if (FireSound)
	{
		// Lower pitch than the beam so a torpedo launch reads as a heavier "thunk".
		UGameplayStatics::PlaySound2D(this, FireSound, 1.f, FMath::FRandRange(0.6f, 0.72f));
	}

	UE_LOG(LogTemp, Log, TEXT("[Torpedo] LAUNCHED at %s (%d rounds left)"),
		*TargetActor->GetName(), Ammo);
	return true;
}

void UTorpedoLauncherComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (ReloadTimer > 0.f)
	{
		ReloadTimer = FMath::Max(0.f, ReloadTimer - DeltaTime);
	}
}
