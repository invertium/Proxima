// SpaceGame — bridge simulator. Explosion flash implementation.

#include "FX/ExplosionFx.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

AExplosionFx::AExplosionFx()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BurstMesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);

	// Engine sphere: 100uu diameter (radius 50) at unit scale.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		Mesh->SetStaticMesh(Sphere.Object);
	}

	static ConstructorHelpers::FObjectFinder<USoundBase> Boom(TEXT("/Game/Audio/S_Explosion.S_Explosion"));
	if (Boom.Succeeded()) { ExplosionSound = Boom.Object; }
}

void AExplosionFx::Activate(const FVector& Location, float PeakRadius, UMaterialInterface* Material, float Life,
	bool bPlaySound)
{
	PeakScale = PeakRadius / 50.f; // sphere radius is 50uu at scale 1
	TotalLife = FMath::Max(Life, 0.01f);
	Remaining = TotalLife;
	if (Material)
	{
		Mesh->SetMaterial(0, Material);
	}
	SetActorLocation(Location);
	Mesh->SetWorldScale3D(FVector::ZeroVector);

	if (bPlaySound && ExplosionSound)
	{
		// Pitch jitter so successive booms don't sound identical (M14).
		UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, Location, 1.f, FMath::FRandRange(0.9f, 1.1f));
	}
}

AExplosionFx* AExplosionFx::Spawn(UWorld* World, const FVector& Location, float PeakRadius,
	UMaterialInterface* Material, float Life, bool bPlaySound)
{
	if (!World) { return nullptr; }
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AExplosionFx* Fx = World->SpawnActor<AExplosionFx>(AExplosionFx::StaticClass(), FTransform::Identity, Params);
	if (Fx)
	{
		Fx->Activate(Location, PeakRadius, Material, Life, bPlaySound);
	}
	return Fx;
}

void AExplosionFx::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Remaining -= DeltaSeconds;
	if (Remaining <= 0.f)
	{
		Destroy();
		return;
	}

	// t 0→1 over life; scale follows sin(t·π): expands to peak then collapses.
	const float T = 1.f - (Remaining / TotalLife);
	const float S = FMath::Sin(T * PI) * PeakScale;
	Mesh->SetWorldScale3D(FVector(S));
}
