// SpaceGame — bridge simulator. Friendly dock station implementation.

#include "World/Station.h"

#include "Components/RadarContactComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AStation::AStation()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// Build the station from basic shapes so it reads as a structure (not the enemy hull with a flat
	// glow): a wide, flat metallic hub disc with a glowing cyan beacon core on top.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> HullMat(TEXT("/Game/Art/Materials/M_Imperial.M_Imperial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlowMat(TEXT("/Game/Art/Materials/M_GlowCyan.M_GlowCyan"));

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionObjectType(ECC_WorldStatic);
	// Engine cylinder is 100uu tall / 100uu wide at scale 1 — make a broad, flat hub disc (~1400 wide).
	Mesh->SetRelativeScale3D(FVector(14.f, 14.f, 3.5f));
	Mesh->SetCastShadow(false);
	if (Cylinder.Succeeded()) { Mesh->SetStaticMesh(Cylinder.Object); }
	if (HullMat.Succeeded())  { Mesh->SetMaterial(0, HullMat.Object); }

	Beacon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beacon"));
	Beacon->SetupAttachment(Mesh);
	Beacon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Beacon->SetCastShadow(false);
	// Sit a small glowing core just above the hub (hub half-height ≈ 175uu at this scale).
	Beacon->SetRelativeScale3D(FVector(0.28f));
	Beacon->SetRelativeLocation(FVector(0.f, 0.f, 62.f));
	if (Sphere.Succeeded()) { Beacon->SetStaticMesh(Sphere.Object); }
	if (GlowMat.Succeeded()) { Beacon->SetMaterial(0, GlowMat.Object); }

	// Friendly, non-targetable blip (cyan-green) so it can't be locked by weapons/science.
	RadarContact = CreateDefaultSubobject<URadarContactComponent>(TEXT("RadarContact"));
	RadarContact->BlipColor = FLinearColor(0.3f, 1.0f, 0.7f, 1.0f);
	RadarContact->bTargetable = false;
}
