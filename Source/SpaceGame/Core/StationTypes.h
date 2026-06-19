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
