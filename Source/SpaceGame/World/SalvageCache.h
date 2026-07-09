// SpaceGame — bridge simulator. Free-floating salvage pickup (M27 travel events).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SalvageCache.generated.h"

class UStaticMeshComponent;
class URadarContactComponent;

/**
 * ASalvageCache — a drifting cargo pod spawned by the sector director's salvage event (M27).
 * Shows as an amber neutral blip on the radar; the director collects it by proximity and pays
 * out credits or a torpedo restock. Purely passive — no tick, no collision gameplay.
 */
UCLASS()
class SPACEGAME_API ASalvageCache : public AActor
{
	GENERATED_BODY()

public:
	ASalvageCache();

protected:
	UPROPERTY(VisibleAnywhere, Category = "Salvage")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Amber neutral blip: visible on the radar, but not a weapons/science lock. */
	UPROPERTY(VisibleAnywhere, Category = "Salvage")
	TObjectPtr<URadarContactComponent> RadarContact;
};
