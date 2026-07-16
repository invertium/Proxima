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
	float InDamage, float InSpeed, float InLifetime)
{
	Target = InTarget;
	FxMaterial = Material;
	Damage = InDamage;
	Speed = InSpeed;
	MaxLifetime = InLifetime;
	LastTargetLoc = InTarget ? InTarget->GetActorLocation() : Start;

	// Launch bearing: point at the target now; the limited-turn steering takes over from here.
	Heading = (LastTargetLoc - Start).GetSafeNormal();
	if (Heading.IsNearlyZero()) { Heading = FVector::ForwardVector; }

	if (Material)
	{
		Mesh->SetMaterial(0, Material);
	}
	SetActorLocation(Start);
	Mesh->SetWorldScale3D(FVector(1.6f)); // ~160uu glowing round
}

ATorpedoProjectile* ATorpedoProjectile::Spawn(UWorld* World, const FVector& Start, AActor* Target,
	UMaterialInterface* Material, float Damage, float Speed, float Lifetime)
{
	if (!World) { return nullptr; }
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATorpedoProjectile* Torp = World->SpawnActor<ATorpedoProjectile>(
		ATorpedoProjectile::StaticClass(), FTransform(Start), Params);
	if (Torp)
	{
		Torp->Activate(Start, Target, Material, Damage, Speed, Lifetime);
	}
	return Torp;
}

void ATorpedoProjectile::Detonate(const FVector& At)
{
	// Big burst straight to hull (torpedoes bypass shield mitigation, M17) — but only if the
	// target is actually inside the blast. A torpedo that times out because the target outran
	// it fizzles harmlessly instead of landing its payload from across the map (M26: enemy
	// volleys are evadable, and the same honesty applies to the player's shots).
	if (AActor* Hit = Target.Get())
	{
		const float Miss = FVector::Dist(Hit->GetActorLocation(), At);
		if (Miss <= BlastRadius)
		{
			if (UHealthComponent* Health = Hit->FindComponentByClass<UHealthComponent>())
			{
				Health->ApplyDamage(Damage, /*bBypassShield=*/true);
			}
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[Torpedo] fizzled %.0f uu short of %s — evaded"),
				Miss, *Hit->GetName());
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

	// Steer toward the target, but only at TurnRateDeg/s — the torpedo can't snap around to chase
	// a warp jump or a hard juke (issue #2). If it can't turn tight enough it flies straight past
	// and fizzles at MaxLifetime (Detonate only lands the payload within BlastRadius).
	const FVector Desired = ToTarget.GetSafeNormal();
	if (!Desired.IsNearlyZero())
	{
		Heading = FMath::VInterpNormalRotationTo(Heading, Desired, DeltaSeconds, TurnRateDeg);
	}
	SetActorLocation(Loc + Heading * Speed * DeltaSeconds);
	SetActorRotation(Heading.Rotation()); // orient along travel (cosmetic for the sphere slug)
}
