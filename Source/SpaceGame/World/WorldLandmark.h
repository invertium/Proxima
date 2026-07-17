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

	// --- Gravity (issues #1/#3): each body has a mass that gently pulls nearby ships ---

	/** The "predefined mass parameter" (issue #1) set at spawn from the body's kind + scale — drives
	 *  both the reach and the strength of its gravity well. */
	float GetGravityMass() const { return GravityMass; }

	/** Range (uu) at which this body's gravity fades to nothing. */
	float GetInfluenceRadius() const { return InfluenceRadius; }

	/** Peak pull speed (uu/s) applied at the body's surface, easing to 0 at InfluenceRadius. Kept
	 *  well below ship thrust so the pull is a nudge you can always power out of (issue #3). */
	float GetPeakPull() const { return PeakPull; }

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

	/** Gravity well parameters, derived from the body's kind + scale in Setup (issues #1/#3). */
	float GravityMass = 0.f;
	float InfluenceRadius = 0.f;
	float PeakPull = 0.f;
};
