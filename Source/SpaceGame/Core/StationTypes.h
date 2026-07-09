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

/**
 * Campaign difficulty (M30), chosen at New Game and persisted in the save. Scales hostile
 * firepower and durability around the Captain baseline.
 */
UENUM(BlueprintType)
enum class EDifficulty : uint8
{
	Ensign  UMETA(DisplayName = "Ensign"),  // softer hostiles
	Captain UMETA(DisplayName = "Captain"), // the tuned baseline
	Admiral UMETA(DisplayName = "Admiral")  // harder-hitting, tougher hostiles
};

/**
 * A station contract's kind (M28). One may be active at a time; it persists in the save.
 */
UENUM(BlueprintType)
enum class EContractType : uint8
{
	None     UMETA(DisplayName = "None"),
	Bounty   UMETA(DisplayName = "Bounty"),   // kill a named ship loitering at a landmark
	Patrol   UMETA(DisplayName = "Patrol"),   // visit two named landmarks
	Delivery UMETA(DisplayName = "Delivery")  // fly cargo to a landmark, return, dock
};

/**
 * Player ship the campaign is being played with (chosen at New Game, persisted in the save).
 * Both reuse existing art; they trade speed for toughness (applied in ASpaceship::BeginPlay).
 */
UENUM(BlueprintType)
enum class EPlayerShipType : uint8
{
	Interceptor UMETA(DisplayName = "Interceptor"), // fast, lighter hull (starter)
	Cruiser     UMETA(DisplayName = "Cruiser"),     // slower, tankier, harder-hitting (starter)
	Corvette    UMETA(DisplayName = "Corvette"),    // glass cannon: very fast, fragile (drydock buy)
	Gunboat     UMETA(DisplayName = "Gunboat")      // heavy hull + big guns, ponderous (drydock buy)
};
