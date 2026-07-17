// SpaceGame — bridge simulator. Gentle celestial gravity (issues #1/#3).

#pragma once

#include "CoreMinimal.h"

class UWorld;
class AWorldLandmark;

/**
 * Celestial gravity: sums the pull of every AWorldLandmark's mass-driven well into a drift velocity
 * (uu/s) toward the bodies, applied to the player ship and enemies each tick. Deliberately gentle —
 * capped well below ship thrust — so it's a nudge you can always power out of, never a trap and never
 * damage (per issue #3's answer). Kept on the combat plane (Z ignored), like the rest of the sim.
 */
namespace GravityField
{
	/** Planar pull velocity (uu/s) at a world location, summed over all landmark gravity wells. */
	FVector PullVelocityAt(const UWorld* World, const FVector& Location);

	/** The single strongest body pulling on Location (for the orbit assist, issue #6), or null.
	 *  OutDistance/OutCenter describe that body's planar range + centre when non-null. */
	AWorldLandmark* DominantBody(const UWorld* World, const FVector& Location,
		float& OutDistance, FVector& OutCenter);

	/** Push Location out of any landmark it has sunk inside — the bodies have no collision of their
	 *  own, so gravity-drifted enemies would otherwise vanish inside an opaque planet (review P2).
	 *  Returns a location kept at least Margin beyond each body's surface. Planar. */
	FVector ClampOutsideBodies(const UWorld* World, const FVector& Location, float Margin);
}
