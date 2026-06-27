// SpaceGame — bridge simulator. Campaign game instance implementation.

#include "Core/SpaceGameInstance.h"

#include "Core/ShipCatalogue.h"
#include "Core/SpaceSaveGame.h"
#include "Core/UpgradeCatalogue.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	// Mission count for the campaign. The mission table itself lands in M18.4
	// (UMissionSubsystem); this is the single source of truth for clamping until then.
	constexpr int32 CampaignMissionCount = 3;
}

void USpaceGameInstance::ResetCampaign()
{
	MissionIndex = 0;
	Credits = 0;
	XP = 0;
	UpgradeTiers.Reset();
	OwnedShips.Reset(); // starters stay owned implicitly via the catalogue
	UE_LOG(LogTemp, Log, TEXT("[Campaign] Reset to mission 0 (wallet + upgrades + hangar cleared)"));
}

bool USpaceGameInstance::BuyUpgrade(FName UpgradeId)
{
	const FUpgradeDef* Def = UpgradeCatalogue::Find(UpgradeId);
	if (!Def)
	{
		return false;
	}
	const int32 Tier = GetUpgradeTier(UpgradeId);
	if (Tier >= Def->MaxTier)
	{
		UE_LOG(LogTemp, Log, TEXT("[Drydock] %s already maxed (tier %d)"), *UpgradeId.ToString(), Tier);
		return false;
	}
	if (GetRank() < UpgradeCatalogue::NextRankReq(*Def, Tier))
	{
		UE_LOG(LogTemp, Log, TEXT("[Drydock] %s tier %d needs rank %d (have %d)"),
			*UpgradeId.ToString(), Tier + 1, UpgradeCatalogue::NextRankReq(*Def, Tier), GetRank());
		return false;
	}
	const int32 Cost = UpgradeCatalogue::NextCost(*Def, Tier);
	if (!SpendCredits(Cost))
	{
		UE_LOG(LogTemp, Log, TEXT("[Drydock] %s tier %d costs %d (have %d)"),
			*UpgradeId.ToString(), Tier + 1, Cost, Credits);
		return false;
	}
	UpgradeTiers.Add(UpgradeId, Tier + 1);
	SaveCampaign();
	UE_LOG(LogTemp, Log, TEXT("[Drydock] Bought %s -> tier %d (-%d cr)"), *UpgradeId.ToString(), Tier + 1, Cost);
	return true;
}

bool USpaceGameInstance::OwnsShip(EPlayerShipType Ship) const
{
	const FShipDef* Def = ShipCatalogue::Find(Ship);
	if (Def && Def->Cost == 0) { return true; } // starters are always owned
	return OwnedShips.Contains(Ship);
}

bool USpaceGameInstance::BuyShip(EPlayerShipType Ship)
{
	const FShipDef* Def = ShipCatalogue::Find(Ship);
	if (!Def || Def->Cost == 0 || OwnsShip(Ship))
	{
		return false;
	}
	if (GetRank() < Def->RankReq)
	{
		UE_LOG(LogTemp, Log, TEXT("[Hangar] %s needs rank %d (have %d)"), *Def->Name, Def->RankReq, GetRank());
		return false;
	}
	if (!SpendCredits(Def->Cost))
	{
		UE_LOG(LogTemp, Log, TEXT("[Hangar] %s costs %d (have %d)"), *Def->Name, Def->Cost, Credits);
		return false;
	}
	OwnedShips.AddUnique(Ship);
	SaveCampaign();
	UE_LOG(LogTemp, Log, TEXT("[Hangar] Bought %s (-%d cr)"), *Def->Name, Def->Cost);
	return true;
}

bool USpaceGameInstance::SelectShip(EPlayerShipType Ship)
{
	if (!OwnsShip(Ship))
	{
		return false;
	}
	PlayerShip = Ship;
	SaveCampaign();
	UE_LOG(LogTemp, Log, TEXT("[Hangar] Active ship -> %d"), (int32)Ship);
	return true;
}

void USpaceGameInstance::AddReward(int32 InCredits, int32 InXP)
{
	Credits = FMath::Max(0, Credits + FMath::Max(0, InCredits));
	XP = FMath::Max(0, XP + FMath::Max(0, InXP));
	UE_LOG(LogTemp, Log, TEXT("[Campaign] +%d cr +%d xp -> %d cr, %d xp (rank %d)"),
		InCredits, InXP, Credits, XP, GetRank());
}

bool USpaceGameInstance::SpendCredits(int32 Amount)
{
	if (Amount < 0 || Credits < Amount)
	{
		return false;
	}
	Credits -= Amount;
	UE_LOG(LogTemp, Log, TEXT("[Campaign] -%d cr -> %d cr"), Amount, Credits);
	return true;
}

int32 USpaceGameInstance::AdvanceMission()
{
	MissionIndex = FMath::Clamp(MissionIndex + 1, 0, CampaignMissionCount - 1);
	UE_LOG(LogTemp, Log, TEXT("[Campaign] Advance -> mission %d"), MissionIndex);
	return MissionIndex;
}

bool USpaceGameInstance::SaveCampaign()
{
	USpaceSaveGame* Save = Cast<USpaceSaveGame>(
		UGameplayStatics::CreateSaveGameObject(USpaceSaveGame::StaticClass()));
	if (!Save)
	{
		return false;
	}
	Save->MissionIndex = MissionIndex;
	Save->PlayerShip = PlayerShip;
	Save->Credits = Credits;
	Save->XP = XP;
	Save->UpgradeTiers = UpgradeTiers;
	Save->OwnedShips = OwnedShips;
	const bool bOk = UGameplayStatics::SaveGameToSlot(Save, SlotName(), 0);
	UE_LOG(LogTemp, Log, TEXT("[Campaign] Save %s (mission %d, ship %d)"),
		bOk ? TEXT("OK") : TEXT("FAILED"), MissionIndex, (int32)PlayerShip);
	return bOk;
}

bool USpaceGameInstance::LoadCampaign()
{
	if (!HasSave())
	{
		return false;
	}
	USpaceSaveGame* Save = Cast<USpaceSaveGame>(
		UGameplayStatics::LoadGameFromSlot(SlotName(), 0));
	if (!Save)
	{
		return false;
	}
	MissionIndex = FMath::Clamp(Save->MissionIndex, 0, CampaignMissionCount - 1);
	PlayerShip = Save->PlayerShip;
	Credits = FMath::Max(0, Save->Credits);
	XP = FMath::Max(0, Save->XP);
	UpgradeTiers = Save->UpgradeTiers;
	OwnedShips = Save->OwnedShips;
	UE_LOG(LogTemp, Log, TEXT("[Campaign] Loaded (mission %d, ship %d, %d cr, %d xp, %d upgrades, %d ships)"),
		MissionIndex, (int32)PlayerShip, Credits, XP, UpgradeTiers.Num(), OwnedShips.Num());
	return true;
}

bool USpaceGameInstance::HasSave() const
{
	return UGameplayStatics::DoesSaveGameExist(SlotName(), 0);
}

int32 USpaceGameInstance::GetMissionCount()
{
	return CampaignMissionCount;
}
