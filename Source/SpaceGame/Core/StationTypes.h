// SpaceGame — bridge simulator. Shared station enum (DECISIONS D6/D10).

#pragma once

#include "CoreMinimal.h"
#include "StationTypes.generated.h"

/**
 * Bridge stations in the vertical slice. Single-machine: one player switches the
 * active console with number keys 1/2/3 (D10). Science/Comms come later.
 */
UENUM(BlueprintType)
enum class EStation : uint8
{
	Helm        UMETA(DisplayName = "Helm"),
	Weapons     UMETA(DisplayName = "Weapons"),
	Engineering UMETA(DisplayName = "Engineering")
};

/**
 * Outcome state of the current encounter, surfaced to every console (and the LAN web
 * stations) so the whole crew knows whether the game is live, won, or lost.
 */
UENUM(BlueprintType)
enum class EGamePhase : uint8
{
	Playing UMETA(DisplayName = "Playing"),
	Victory UMETA(DisplayName = "Victory"),
	Defeat  UMETA(DisplayName = "Defeat")
};
