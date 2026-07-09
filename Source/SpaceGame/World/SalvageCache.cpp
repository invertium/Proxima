// SpaceGame — bridge simulator. Salvage pickup implementation.

#include "World/SalvageCache.h"

#include "Components/RadarContactComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ASalvageCache::ASalvageCache()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CacheMesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(
		TEXT("/Game/Art/Materials/M_GlowOrange.M_GlowOrange"));
	if (Cube.Succeeded()) { Mesh->SetStaticMesh(Cube.Object); }
	if (Glow.Succeeded()) { Mesh->SetMaterial(0, Glow.Object); }
	Mesh->SetRelativeScale3D(FVector(2.2f));
	Mesh->SetRelativeRotation(FRotator(20.f, 35.f, 15.f)); // tumbled attitude so it reads as adrift

	RadarContact = CreateDefaultSubobject<URadarContactComponent>(TEXT("RadarContact"));
	RadarContact->BlipColor = FLinearColor(1.0f, 0.8f, 0.3f, 1.f);
	RadarContact->bTargetable = false;
}
