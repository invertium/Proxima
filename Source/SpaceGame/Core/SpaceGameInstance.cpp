// SpaceGame — bridge simulator. Campaign game instance implementation.

#include "Core/SpaceGameInstance.h"

#include "Core/SpaceSaveGame.h"
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
	UE_LOG(LogTemp, Log, TEXT("[Campaign] Reset to mission 0"));
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
	UE_LOG(LogTemp, Log, TEXT("[Campaign] Loaded (mission %d, ship %d)"), MissionIndex, (int32)PlayerShip);
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
