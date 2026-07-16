// SpaceGame — bridge simulator. Gentle celestial gravity implementation.

#include "World/GravityField.h"

#include "World/WorldLandmark.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

// Celestial gravity on/off (issue follow-up: toggleable). Default on; flip live from the console
// (`sg.Gravity 0`) or the web Helm GRAVITY button (/api/toggle?what=gravity).
static TAutoConsoleVariable<int32> CVarGravity(
	TEXT("sg.Gravity"), 1,
	TEXT("Celestial gravity: gentle pull toward planets/suns (0=off, 1=on)."),
	ECVF_Default);

FVector GravityField::PullVelocityAt(const UWorld* World, const FVector& Location)
{
	FVector Pull = FVector::ZeroVector;
	if (!World || CVarGravity.GetValueOnGameThread() == 0) { return Pull; }

	for (TActorIterator<AWorldLandmark> It(World); It; ++It)
	{
		const AWorldLandmark* B = *It;
		if (!IsValid(B)) { continue; }
		const float Infl = B->GetInfluenceRadius();
		if (Infl <= 0.f) { continue; }

		FVector To = B->GetActorLocation() - Location;
		To.Z = 0.f; // combat stays on the plane
		const float Dist = To.Size();
		if (Dist >= Infl || Dist <= 1.f) { continue; }

		// Ease from full pull at the surface to zero at the influence edge (quadratic falloff).
		const float Surface = B->GetBodyRadius();
		const float T = FMath::Clamp(1.f - (Dist - Surface) / FMath::Max(1.f, Infl - Surface), 0.f, 1.f);
		Pull += (To / Dist) * (B->GetPeakPull() * T * T);
	}
	return Pull;
}

AWorldLandmark* GravityField::DominantBody(const UWorld* World, const FVector& Location,
	float& OutDistance, FVector& OutCenter)
{
	AWorldLandmark* Best = nullptr;
	float BestPull = 0.f;
	OutDistance = -1.f;
	OutCenter = FVector::ZeroVector;
	if (!World) { return nullptr; }

	for (TActorIterator<AWorldLandmark> It(World); It; ++It)
	{
		AWorldLandmark* B = *It;
		if (!IsValid(B)) { continue; }
		const float Infl = B->GetInfluenceRadius();
		if (Infl <= 0.f) { continue; }

		FVector To = B->GetActorLocation() - Location;
		To.Z = 0.f;
		const float Dist = To.Size();
		if (Dist >= Infl || Dist <= 1.f) { continue; }

		const float Surface = B->GetBodyRadius();
		const float T = FMath::Clamp(1.f - (Dist - Surface) / FMath::Max(1.f, Infl - Surface), 0.f, 1.f);
		const float Strength = B->GetPeakPull() * T * T;
		if (Strength > BestPull)
		{
			BestPull = Strength;
			Best = B;
			OutDistance = Dist;
			OutCenter = B->GetActorLocation();
		}
	}
	return Best;
}
