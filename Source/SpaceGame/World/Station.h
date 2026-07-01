// SpaceGame — bridge simulator. Friendly dock station (M19 progression).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Station.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class URadarContactComponent;

/**
 * AStation — a friendly starbase the player ship can dock with. It is a passive landmark: a
 * scaled-up hull with a friendly (non-targetable) radar blip and a dock radius. Docking logic
 * lives on ASpaceship (which finds the nearest station); the station just provides the place,
 * the blip, and the range. Spawned by UMissionSubsystem near the player's start each encounter.
 */
UCLASS()
class SPACEGAME_API AStation : public AActor
{
	GENERATED_BODY()

public:
	AStation();

	/** How close (uu) the ship must be to dock. */
	UFUNCTION(BlueprintPure, Category = "Station")
	float GetDockRange() const { return DockRange; }

	/** Callsign shown on the consoles' radar/labels. */
	UFUNCTION(BlueprintPure, Category = "Station")
	const FString& GetStationName() const { return StationName; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Station")
	TObjectPtr<USceneComponent> Root;

	/** Structure hull — a lit metallic hub so it reads as a built structure, not a glowing blob. */
	UPROPERTY(VisibleAnywhere, Category = "Station")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Glowing cyan beacon core atop the hub — the friendly running light. */
	UPROPERTY(VisibleAnywhere, Category = "Station")
	TObjectPtr<UStaticMeshComponent> Beacon;

	/** Friendly, non-targetable radar blip. */
	UPROPERTY(VisibleAnywhere, Category = "Station")
	TObjectPtr<URadarContactComponent> RadarContact;

	/** Dock radius (uu) — the ship must be this close to the hub to dock. Kept snug to the structure. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station")
	float DockRange = 1600.f;

	/** Display callsign. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station")
	FString StationName = TEXT("STARBASE");
};
