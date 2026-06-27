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

	// Reuse the Imperial hull, scaled up and tinted friendly cyan — reads as a big structure.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> HullMesh(TEXT("/Game/Art/Meshes/Imperial.Imperial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> HullMat(TEXT("/Game/Art/Materials/M_GlowCyan.M_GlowCyan"));

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionObjectType(ECC_WorldStatic);
	Mesh->SetRelativeScale3D(FVector(3.0f));
	if (HullMesh.Succeeded()) { Mesh->SetStaticMesh(HullMesh.Object); }
	if (HullMat.Succeeded())  { Mesh->SetMaterial(0, HullMat.Object); }

	// Friendly, non-targetable blip (cyan-green) so it can't be locked by weapons/science.
	RadarContact = CreateDefaultSubobject<URadarContactComponent>(TEXT("RadarContact"));
	RadarContact->BlipColor = FLinearColor(0.3f, 1.0f, 0.7f, 1.0f);
	RadarContact->bTargetable = false;
}
