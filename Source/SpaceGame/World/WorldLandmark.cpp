// SpaceGame — bridge simulator. Open-sector landmark implementation.

#include "World/WorldLandmark.h"

#include "Components/RadarContactComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
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

	if (!Body) { return; }

	// Distinct, planet-like bodies. The sun is a bright emissive star; the home world keeps the detailed
	// Earth material; every other planet uses the tintable M_Planet (a lit sphere with a Fresnel
	// atmosphere rim) coloured to its own hue — so no two read alike and none wear a ship-hull texture.
	const bool bEarthlike = (Color.B > Color.R && Color.B > Color.G && Color.B > 0.8f);
	if (bSun)
	{
		if (UMaterialInterface* Mat = Cast<UMaterialInterface>(StaticLoadObject(
			UMaterialInterface::StaticClass(), nullptr, TEXT("/Game/Art/Materials/M_GlowOrange.M_GlowOrange"))))
		{
			Body->SetMaterial(0, Mat);
		}
	}
	else if (bEarthlike)
	{
		if (UMaterialInterface* Mat = Cast<UMaterialInterface>(StaticLoadObject(
			UMaterialInterface::StaticClass(), nullptr, TEXT("/Game/Art/Materials/M_Earth.M_Earth"))))
		{
			Body->SetMaterial(0, Mat);
		}
	}
	else if (UMaterialInterface* Base = Cast<UMaterialInterface>(StaticLoadObject(
		UMaterialInterface::StaticClass(), nullptr, TEXT("/Game/Art/Materials/M_Planet.M_Planet"))))
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		Body->SetMaterial(0, MID);
	}
}
