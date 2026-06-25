// SpaceGame — bridge simulator. GameMode implementation.

#include "Core/SpaceGameMode.h"

#include "Core/BridgePlayerController.h"
#include "Kismet/GameplayStatics.h"

ASpaceGameMode::ASpaceGameMode()
{
	PlayerControllerClass = ABridgePlayerController::StaticClass();

	// The player's ship is placed in the level and auto-possesses Player0;
	// don't spawn a DefaultPawn over it.
	DefaultPawnClass = nullptr;
}

void ASpaceGameMode::SetPhase(EGamePhase NewPhase)
{
	if (Phase == NewPhase)
	{
		return;
	}
	Phase = NewPhase;

	const TCHAR* Name =
		NewPhase == EGamePhase::Victory ? TEXT("Victory") :
		NewPhase == EGamePhase::Defeat  ? TEXT("Defeat")  :
		                                  TEXT("Playing");
	UE_LOG(LogTemp, Log, TEXT("[Game] Phase -> %s"), Name);
}

void ASpaceGameMode::RestartEncounter()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Make sure a paused end-screen doesn't carry into the reload, then reopen the current
	// level — this respawns the ship + hostiles and recreates the world subsystems (the LAN
	// station server rebinds cleanly), so it serves as both New Game and Restart for the slice.
	UGameplayStatics::SetGamePaused(World, false);
	const FString LevelName = UGameplayStatics::GetCurrentLevelName(World, true);
	UE_LOG(LogTemp, Log, TEXT("[Game] Restarting encounter -> reloading level '%s'"), *LevelName);
	UGameplayStatics::OpenLevel(World, FName(*LevelName));
}
