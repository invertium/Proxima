// SpaceGame — bridge simulator. Drifting hull-debris chunk implementation.

#include "FX/Debris.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ADebris::ADebris()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChunkMesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);

	// A cube, scaled jaggedly per-chunk so the pieces read as broken hull, not boxes.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded()) { Mesh->SetStaticMesh(Cube.Object); }
}

void ADebris::Activate(const FVector& Location, const FVector& InVelocity, UMaterialInterface* Material,
	float Scale, float Life)
{
	if (Material) { Mesh->SetMaterial(0, Material); }

	// Jagged, non-uniform base scale (cube native size is 100uu, so Scale ~0.3 -> ~30uu chunk).
	PeakScale = FVector(Scale * FMath::FRandRange(0.5f, 1.2f),
	                    Scale * FMath::FRandRange(0.5f, 1.2f),
	                    Scale * FMath::FRandRange(0.5f, 1.2f));
	SetActorLocation(Location);
	SetActorRotation(FRotator(FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f)));
	Mesh->SetWorldScale3D(PeakScale);

	Velocity = InVelocity;
	Spin = FRotator(FMath::FRandRange(-90.f, 90.f), FMath::FRandRange(-90.f, 90.f), FMath::FRandRange(-90.f, 90.f));
	TotalLife = FMath::Max(Life, 0.1f);
	Remaining = TotalLife;
}

ADebris* ADebris::Spawn(UWorld* World, const FVector& Location, const FVector& Velocity,
	UMaterialInterface* Material, float Scale, float Life)
{
	if (!World) { return nullptr; }
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADebris* D = World->SpawnActor<ADebris>(ADebris::StaticClass(), FTransform::Identity, Params);
	if (D) { D->Activate(Location, Velocity, Material, Scale, Life); }
	return D;
}

void ADebris::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Remaining -= DeltaSeconds;
	if (Remaining <= 0.f)
	{
		Destroy();
		return;
	}

	// Drift (with a touch of drag) and tumble.
	Velocity *= FMath::Clamp(1.f - 0.15f * DeltaSeconds, 0.f, 1.f);
	AddActorWorldOffset(Velocity * DeltaSeconds);
	AddActorLocalRotation(Spin * DeltaSeconds);

	// Shrink away over the final 25% of life so chunks vanish gracefully.
	const float Frac = Remaining / TotalLife;
	if (Frac < 0.25f)
	{
		Mesh->SetWorldScale3D(PeakScale * (Frac / 0.25f));
	}
}
