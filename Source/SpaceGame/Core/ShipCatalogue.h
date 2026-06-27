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
		static const FString Insurgent = TEXT("/Game/Art/Meshes/Insurgent.Insurgent");
		static const FString Imperial  = TEXT("/Game/Art/Meshes/Imperial.Imperial");
		static const TArray<FShipDef> Roster = {
			// Type,                  Name,          Blurb,                       Mesh,      Material,                                      Scale  Spd    Acc    Turn  Hull  Beam Rchg  Torp Cost Rank
			{ EPlayerShipType::Interceptor, TEXT("Interceptor"), TEXT("Fast, agile, light hull."),  Insurgent, TEXT("/Game/Art/Materials/M_Insurgent.M_Insurgent"), 0.60f, 2100.f, 1500.f, 75.f,  80.f, 20.f, 0.55f, 3,    0, 0 },
			{ EPlayerShipType::Cruiser,     TEXT("Cruiser"),     TEXT("Slow, tough, hits hard."),   Insurgent, TEXT("/Game/Art/Materials/M_PlayerHull.M_PlayerHull"),0.95f, 1300.f,  900.f, 42.f, 160.f, 34.f, 0.30f, 6,    0, 0 },
			{ EPlayerShipType::Corvette,    TEXT("Corvette"),    TEXT("Glass cannon: blistering speed, paper hull."), Insurgent, TEXT("/Game/Art/Materials/M_GlowOrange.M_GlowOrange"), 0.50f, 2500.f, 1800.f, 92.f, 60.f, 16.f, 0.75f, 2, 1200, 2 },
			{ EPlayerShipType::Gunboat,     TEXT("Gunboat"),     TEXT("Heavy hull and big guns, ponderous turn."),    Imperial,  TEXT("/Game/Art/Materials/M_PlayerHull.M_PlayerHull"), 0.80f, 1100.f,  800.f, 36.f, 240.f, 42.f, 0.26f, 8, 1800, 3 },
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
