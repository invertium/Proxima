// SpaceGame — bridge simulator. Beam effect implementation.

#include "FX/BeamFx.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

ABeamFx::ABeamFx()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BeamMesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);

	// Engine cylinder: 100uu tall along local +Z, 100uu diameter, centred.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cyl.Succeeded())
	{
		Mesh->SetStaticMesh(Cyl.Object);
	}
}

void ABeamFx::Activate(const FVector& Start, const FVector& End, UMaterialInterface* Material, float Radius, float Life)
{
	const FVector Delta = End - Start;
	const float Length = Delta.Size();
	BaseRadius = Radius;
	Mid = (Start + End) * 0.5f;
	Orient = FRotationMatrix::MakeFromZ(Delta.GetSafeNormal()).Rotator();
	LengthScale = Length / 100.f;
	TotalLife = FMath::Max(Life, 0.01f);
	Remaining = TotalLife;

	if (Material)
	{
		Mesh->SetMaterial(0, Material);
	}
	SetActorLocationAndRotation(Mid, Orient);
	const float XY = (2.f * BaseRadius) / 100.f;
	Mesh->SetWorldScale3D(FVector(XY, XY, LengthScale));
}

ABeamFx* ABeamFx::Spawn(UWorld* World, const FVector& Start, const FVector& End,
	UMaterialInterface* Material, float Radius, float Life)
{
	if (!World) { return nullptr; }
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABeamFx* Fx = World->SpawnActor<ABeamFx>(ABeamFx::StaticClass(), FTransform::Identity, Params);
	if (Fx)
	{
		Fx->Activate(Start, End, Material, Radius, Life);
	}
	return Fx;
}

void ABeamFx::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Remaining -= DeltaSeconds;
	if (Remaining <= 0.f)
	{
		Destroy();
		return;
	}

	// Fade by thinning the beam toward its end of life (keeps the length fixed).
	const float Alpha = Remaining / TotalLife;
	const float XY = (2.f * BaseRadius * Alpha) / 100.f;
	Mesh->SetWorldScale3D(FVector(XY, XY, LengthScale));
}
