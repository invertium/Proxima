// SpaceGame — bridge simulator. Open-sector landmark implementation.

#include "World/WorldLandmark.h"

#include "Components/RadarContactComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AWorldLandmark::AWorldLandmark()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision); // navigation marker, not a hazard (for now)
	Body->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded()) { Body->SetStaticMesh(Sphere.Object); }

	// Neutral, non-targetable blip so the body shows on radar/map but can't be weapon-locked.
	RadarContact = CreateDefaultSubobject<URadarContactComponent>(TEXT("RadarContact"));
	RadarContact->bTargetable = false;
}

void AWorldLandmark::Setup(const FString& InName, float Scale, const FLinearColor& Color, bool bSun)
{
	LandmarkName = InName;
	if (RadarContact) { RadarContact->BlipColor = Color; }

	// Engine sphere is 100uu across (radius 50) at scale 1. Planets read ~6k across, the sun ~25k.
	const float BodyScale = (bSun ? 250.f : 60.f) * FMath::Max(0.2f, Scale);
	if (Body) { Body->SetWorldScale3D(FVector(BodyScale)); }

	// Distinct bodies from the existing palette (no imports): the sun glows (emissive), planets use
	// LIT surfaces so up close they read as shaded coloured worlds, not white blobs.
	const TCHAR* MatPath;
	if (bSun)
	{
		MatPath = TEXT("/Game/Art/Materials/M_GlowOrange.M_GlowOrange"); // a hot, glowing star
	}
	else if (Color.B > Color.R && Color.B >= Color.G)
	{
		MatPath = TEXT("/Game/Art/Materials/M_Earth.M_Earth");          // blue world (home / earthlike)
	}
	else if (Color.R > 0.55f && Color.G < 0.45f)
	{
		MatPath = TEXT("/Game/Art/Materials/M_Imperial.M_Imperial");    // red / industrial
	}
	else if (Color.R > 0.55f)
	{
		MatPath = TEXT("/Game/Art/Materials/M_PlayerHull.M_PlayerHull"); // amber / rocky
	}
	else
	{
		MatPath = TEXT("/Game/Art/Materials/M_Insurgent.M_Insurgent");  // teal / icy
	}
	if (Body)
	{
		if (UMaterialInterface* Mat = Cast<UMaterialInterface>(
			StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, MatPath)))
		{
			Body->SetMaterial(0, Mat);
		}
	}
}
