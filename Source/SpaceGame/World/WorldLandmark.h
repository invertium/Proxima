// SpaceGame — bridge simulator. Open-sector landmark body (M23).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldLandmark.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class URadarContactComponent;
class UMaterialInterface;

/**
 * AWorldLandmark — a passive celestial body (planet or sun) marking a system in the open sector. It is
 * a big emissive sphere with a non-targetable radar blip and a display name, so the crew can navigate to
 * it on the radar / sector map. Encounters trigger by proximity to these (handled by UMissionSubsystem).
 */
UCLASS()
class SPACEGAME_API AWorldLandmark : public AActor
{
	GENERATED_BODY()

public:
	AWorldLandmark();

	/** Configure this body's size, colour, blip and name (called by the sector director on spawn). */
	void Setup(const FString& InName, float Scale, const FLinearColor& Color, bool bSun);

	UFUNCTION(BlueprintPure, Category = "Landmark")
	const FString& GetLandmarkName() const { return LandmarkName; }

	/** World-space radius (uu) of the body — used by the ship to treat it as a solid obstacle. */
	float GetBodyRadius() const;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Landmark")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Landmark")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Landmark")
	TObjectPtr<URadarContactComponent> RadarContact;

	FString LandmarkName;

	/** World-space radius (uu) of the body, cached from the mesh bounds × scale in Setup. */
	float BodyRadius = 0.f;
};
