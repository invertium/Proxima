// SpaceGame — bridge simulator. Player ship roster (M19.4, data-in-code).

#pragma once

#include "CoreMinimal.h"
#include "Core/StationTypes.h"

/** One player hull: visuals + base stats + (for non-starters) its drydock price. Cost 0 = owned from
 *  the start. ASpaceship::ApplyShipPreset looks up the active type here; the hangar lists them all. */
struct FShipDef
{
	EPlayerShipType Type;
	FString Name;
	FString Blurb;
	FString MeshPath;
	FString MatPath;
	float Scale;
	float MaxSpeed;
	float Acceleration;
	float TurnRate;
	float MaxHull;
	float BeamDamage;
	float BeamRecharge;
	int32 TorpedoAmmo;
	int32 Cost;     // 0 = starter (always owned)
	int32 RankReq;
};

namespace ShipCatalogue
{
	inline const TArray<FShipDef>& Get()
	{
		// Distinct generated hull per role (content-engine TRELLIS, art_src/generated_ships/).
		// Scales calibrated so each hull keeps the effective length of its pre-M25 stand-in
		// (Insurgent 1095 uu / Imperial 1855 uu baselines) — collision + radar sizes unchanged.
		static const TArray<FShipDef> Roster = {
			// Type,                  Name,          Blurb,                       Mesh,      Material,                                      Scale  Spd    Acc    Turn  Hull  Beam Rchg  Torp Cost Rank
			{ EPlayerShipType::Interceptor, TEXT("Interceptor"), TEXT("Fast, agile, light hull."),  TEXT("/Game/Art/Meshes/ShipInterceptor.ShipInterceptor"), TEXT("/Game/Art/Materials/M_ShipInterceptor.M_ShipInterceptor"), 0.70f, 2100.f, 1500.f, 75.f,  80.f, 20.f, 0.55f, 3,    0, 0 },
			{ EPlayerShipType::Cruiser,     TEXT("Cruiser"),     TEXT("Slow, tough, hits hard."),   TEXT("/Game/Art/Meshes/ShipCruiser.ShipCruiser"),         TEXT("/Game/Art/Materials/M_ShipCruiser.M_ShipCruiser"),         1.04f, 1300.f,  900.f, 42.f, 160.f, 34.f, 0.30f, 6,    0, 0 },
			{ EPlayerShipType::Corvette,    TEXT("Corvette"),    TEXT("Glass cannon: blistering speed, paper hull."), TEXT("/Game/Art/Meshes/ShipCorvette.ShipCorvette"), TEXT("/Game/Art/Materials/M_ShipCorvette.M_ShipCorvette"), 0.59f, 2500.f, 1800.f, 92.f, 60.f, 16.f, 0.75f, 2, 1200, 2 },
			{ EPlayerShipType::Gunboat,     TEXT("Gunboat"),     TEXT("Heavy hull and big guns, ponderous turn."),    TEXT("/Game/Art/Meshes/ShipGunboat.ShipGunboat"),   TEXT("/Game/Art/Materials/M_ShipGunboat.M_ShipGunboat"),   1.47f, 1100.f,  800.f, 36.f, 240.f, 42.f, 0.26f, 8, 1800, 3 },
		};
		return Roster;
	}

	inline const FShipDef* Find(EPlayerShipType Type)
	{
		for (const FShipDef& S : Get())
		{
			if (S.Type == Type) { return &S; }
		}
		return nullptr;
	}
}
