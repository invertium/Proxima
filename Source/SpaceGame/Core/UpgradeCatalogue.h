// SpaceGame — bridge simulator. Drydock upgrade catalogue (M19.3, data-in-code).

#pragma once

#include "CoreMinimal.h"

/** Which ship stat an upgrade improves (mapped to a component field in ASpaceship::ApplyShipPreset). */
enum class EUpgradeStat : uint8
{
	BeamDamage,    // UWeaponComponent::BeamDamage
	BeamRecharge,  // UWeaponComponent::BaseRechargeRate
	FireArc,       // UWeaponComponent::FireArcDeg
	MaxHull,       // UHealthComponent::MaxHull
	MaxShield,     // UHealthComponent::MaxShield
	TorpedoAmmo,   // UTorpedoLauncherComponent::MaxAmmo
	ReactorBudget, // UPowerComponent::ReactorBudget
	StrafeSpeed,   // UShipMovementComponent::MaxStrafeSpeed (issue #7: bought side-thrusters)
	Turret         // UWeaponComponent::TurretDamage (issue #5: bought auto-turret)
};

/** One purchasable upgrade path: tiers add MagnitudePerTier to a stat, with escalating cost + rank gate. */
struct FUpgradeDef
{
	FName Id;
	FString DisplayName;
	FString Unit;            // shown after the magnitude (e.g. "dmg", "°", "hull")
	EUpgradeStat Stat;
	float MagnitudePerTier;
	int32 MaxTier;
	int32 BaseCost;          // tier (t+1) costs BaseCost*(t+1); rank (t+1) is required to buy it
};

namespace UpgradeCatalogue
{
	/** The full catalogue (built once). */
	inline const TArray<FUpgradeDef>& Get()
	{
		static const TArray<FUpgradeDef> Catalogue = {
			{ TEXT("beam_damage"),   TEXT("Beam Damage"),     TEXT("dmg"),   EUpgradeStat::BeamDamage,    8.f,  3, 150 },
			{ TEXT("beam_recharge"), TEXT("Beam Recharge"),   TEXT("/s"),    EUpgradeStat::BeamRecharge,  0.15f, 3, 150 },
			{ TEXT("fire_arc"),      TEXT("Targeting Arc"),   TEXT("°"),EUpgradeStat::FireArc,       15.f, 3, 120 },
			{ TEXT("hull"),          TEXT("Hull Plating"),    TEXT("hull"),  EUpgradeStat::MaxHull,       40.f, 3, 200 },
			{ TEXT("shields"),       TEXT("Shield Capacity"), TEXT("shld"),  EUpgradeStat::MaxShield,     30.f, 3, 200 },
			{ TEXT("torpedo"),       TEXT("Torpedo Tubes"),   TEXT("rds"),   EUpgradeStat::TorpedoAmmo,   2.f,  3, 180 },
			{ TEXT("reactor"),       TEXT("Reactor Output"),  TEXT("pwr"),   EUpgradeStat::ReactorBudget, 0.5f, 3, 250 },
			// One-time modules (MaxTier 1) the starter hull doesn't carry — buy once to unlock.
			{ TEXT("strafe"),        TEXT("Maneuvering Thrusters"), TEXT("uu/s"), EUpgradeStat::StrafeSpeed, 950.f, 1, 160 },
			{ TEXT("turret"),        TEXT("Auto-Turret"),     TEXT("dmg"),   EUpgradeStat::Turret,        12.f, 1, 240 },
		};
		return Catalogue;
	}

	inline const FUpgradeDef* Find(FName Id)
	{
		for (const FUpgradeDef& U : Get())
		{
			if (U.Id == Id) { return &U; }
		}
		return nullptr;
	}

	/** Credit cost to buy the next tier from CurrentTier (0-based). */
	inline int32 NextCost(const FUpgradeDef& U, int32 CurrentTier)
	{
		return U.BaseCost * (CurrentTier + 1);
	}

	/** Crew rank required to buy the next tier from CurrentTier (tier 1 needs rank 1, tier 2 rank 2, …). */
	inline int32 NextRankReq(const FUpgradeDef& U, int32 CurrentTier)
	{
		return CurrentTier + 1;
	}
}
