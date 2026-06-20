// SpaceGame — bridge simulator. Marker: actors with this show as radar blips.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RadarContactComponent.generated.h"

/**
 * URadarContactComponent — tag component. Any actor carrying it is drawn as a blip
 * on the tactical radar (URadarWidget). Enemies (M9) add it; the player's own ship
 * does not (it is the radar centre). BlipColor lets friend/foe read at a glance.
 */
UCLASS(ClassGroup = (SpaceGame), meta = (BlueprintSpawnableComponent))
class SPACEGAME_API URadarContactComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Colour of this contact's blip on the radar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radar")
	FLinearColor BlipColor = FLinearColor(1.0f, 0.25f, 0.2f, 1.0f); // hostile red
};
