// SpaceGame — bridge simulator. Campaign-persistent game instance (M18).

#pragma once

#include "CoreMinimal.h"
#include "Core/StationTypes.h"
#include "Engine/GameInstance.h"
#include "SpaceGameInstance.generated.h"

/**
 * USpaceGameInstance — survives level loads, so it carries the campaign across the menu,
 * each mission encounter, and reloads. Holds the live campaign state (reached mission +
 * chosen ship) and owns the single-slot save: New Game resets it, Continue loads it, the
 * pause menu / mission-complete saves it. The mission subsystem reads MissionIndex to know
 * which encounter to build; the player ship reads PlayerShip to pick its variant.
 */
UCLASS()
class SPACEGAME_API USpaceGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Campaign")
	int32 GetMissionIndex() const { return MissionIndex; }

	UFUNCTION(BlueprintCallable, Category = "Campaign")
	void SetMissionIndex(int32 InIndex) { MissionIndex = FMath::Max(0, InIndex); }

	UFUNCTION(BlueprintPure, Category = "Campaign")
	EPlayerShipType GetPlayerShip() const { return PlayerShip; }

	UFUNCTION(BlueprintCallable, Category = "Campaign")
	void SetPlayerShip(EPlayerShipType InShip) { PlayerShip = InShip; }

	/** Reset to a fresh campaign (mission 0); does not touch the save on disk. */
	UFUNCTION(BlueprintCallable, Category = "Campaign")
	void ResetCampaign();

	/** Advance to the next mission (clamped at the last); returns the new index. */
	UFUNCTION(BlueprintCallable, Category = "Campaign")
	int32 AdvanceMission();

	/** Write the live campaign state to the save slot. Returns true on success. */
	UFUNCTION(BlueprintCallable, Category = "Campaign")
	bool SaveCampaign();

	/** Load the save slot into the live state. Returns true if a save existed and loaded. */
	UFUNCTION(BlueprintCallable, Category = "Campaign")
	bool LoadCampaign();

	/** True if a campaign save exists on disk (gates the menu's Continue). */
	UFUNCTION(BlueprintPure, Category = "Campaign")
	bool HasSave() const;

	/** Number of missions in the campaign (last index = MissionCount-1). */
	UFUNCTION(BlueprintPure, Category = "Campaign")
	static int32 GetMissionCount();

	// --- Progression economy (M19): salvage credits + XP-driven rank ---

	/** Spendable salvage credits banked across the campaign. */
	UFUNCTION(BlueprintPure, Category = "Campaign|Economy")
	int32 GetCredits() const { return Credits; }

	/** Lifetime XP earned (drives GetRank). */
	UFUNCTION(BlueprintPure, Category = "Campaign|Economy")
	int32 GetXP() const { return XP; }

	/** Crew rank derived from XP (1-based; every XpPerRank XP is one rank). Gates upgrade tiers. */
	UFUNCTION(BlueprintPure, Category = "Campaign|Economy")
	int32 GetRank() const { return 1 + XP / XpPerRank; }

	/** Bank a kill/mission reward (credits + XP). */
	UFUNCTION(BlueprintCallable, Category = "Campaign|Economy")
	void AddReward(int32 InCredits, int32 InXP);

	/** Try to spend credits (returns false if too few; deducts on success). */
	UFUNCTION(BlueprintCallable, Category = "Campaign|Economy")
	bool SpendCredits(int32 Amount);

	// --- Drydock upgrades (M19.3) ---

	/** Owned tier (0 = none) for a catalogue upgrade id. */
	UFUNCTION(BlueprintPure, Category = "Campaign|Upgrades")
	int32 GetUpgradeTier(FName UpgradeId) const { return UpgradeTiers.FindRef(UpgradeId); }

	/** Attempt to buy the next tier of an upgrade: checks the catalogue, max tier, rank, and credits;
	 *  on success deducts credits, bumps the tier, and persists. Returns true if bought. */
	UFUNCTION(BlueprintCallable, Category = "Campaign|Upgrades")
	bool BuyUpgrade(FName UpgradeId);

private:
	UPROPERTY()
	int32 MissionIndex = 0;

	UPROPERTY()
	EPlayerShipType PlayerShip = EPlayerShipType::Interceptor;

	UPROPERTY()
	int32 Credits = 0;

	UPROPERTY()
	int32 XP = 0;

	/** Owned upgrade tiers keyed by catalogue id (absent = tier 0). */
	UPROPERTY()
	TMap<FName, int32> UpgradeTiers;

	/** XP needed per rank step (rank = 1 + XP/XpPerRank). */
	static constexpr int32 XpPerRank = 400;

	static const TCHAR* SlotName() { return TEXT("campaign"); }
};
