// SpaceGame — bridge simulator. Torpedo projectile implementation.

#include "FX/TorpedoProjectile.h"

#include "Components/HealthComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "FX/ExplosionFx.h"
#include "UObject/ConstructorHelpers.h"

ATorpedoProjectile::ATorpedoProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TorpedoMesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);

	// Engine sphere, scaled down to a small glowing slug.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		Mesh->SetStaticMesh(Sphere.Object);
	}
}

void ATorpedoProjectile::Activate(const FVector& Start, AActor* InTarget, UMaterialInterface* Material,
	float InDamage, float InSpeed)
{
	Target = InTarget;
	FxMaterial = Material;
	Damage = InDamage;
	Speed = InSpeed;
	LastTargetLoc = InTarget ? InTarget->GetActorLocation() : Start;

	if (Material)
	{
		Mesh->SetMaterial(0, Material);
	}
	SetActorLocation(Start);
	Mesh->SetWorldScale3D(FVector(1.6f)); // ~160uu glowing round
}

ATorpedoProjectile* ATorpedoProjectile::Spawn(UWorld* World, const FVector& Start, AActor* Target,
	UMaterialInterface* Material, float Damage, float Speed)
{
	if (!World) { return nullptr; }
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATorpedoProjectile* Torp = World->SpawnActor<ATorpedoProjectile>(
		ATorpedoProjectile::StaticClass(), FTransform(Start), Params);
	if (Torp)
	{
		Torp->Activate(Start, Target, Material, Damage, Speed);
	}
	return Torp;
}

void ATorpedoProjectile::Detonate(const FVector& At)
{
	// Big burst straight to hull (torpedoes bypass shield mitigation, M17).
	if (AActor* Hit = Target.Get())
	{
		if (UHealthComponent* Health = Hit->FindComponentByClass<UHealthComponent>())
		{
			Health->ApplyDamage(Damage, /*bBypassShield=*/true);
		}
	}
	// Orange blast at the impact point (with boom).
	AExplosionFx::Spawn(GetWorld(), At, 600.f, FxMaterial, 0.5f, true);
	Destroy();
}

void ATorpedoProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;
	if (Age >= MaxLifetime)
	{
		Detonate(GetActorLocation());
		return;
	}

	// Home on the target's current position (fall back to its last seen spot if it's gone).
	if (AActor* T = Target.Get())
	{
		LastTargetLoc = T->GetActorLocation();
	}

	const FVector Loc = GetActorLocation();
	const FVector ToTarget = LastTargetLoc - Loc;
	const float Dist = ToTarget.Size();
	if (Dist <= HitRadius)
	{
		Detonate(LastTargetLoc);
		return;
	}

	const FVector Step = ToTarget.GetSafeNormal() * Speed * DeltaSeconds;
	if (Step.Size() >= Dist)
	{
		Detonate(LastTargetLoc);
		return;
	}
	SetActorLocation(Loc + Step);
}
