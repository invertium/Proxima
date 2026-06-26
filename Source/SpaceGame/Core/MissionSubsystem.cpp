// SpaceGame — bridge simulator. Mission/encounter builder implementation.

#include "Core/MissionSubsystem.h"

#include "Core/BridgePlayerController.h"
#include "Core/SpaceGameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Ships/EnemyShip.h"

namespace
{
	// The campaign: escalating fleets built from the three enemy archetypes, each with a short
	// comms script (surfaced on the Science console in M18.6). Keep the size in sync with
	// USpaceGameInstance::GetMissionCount().
	TArray<FMissionDef> BuildCampaign()
	{
		TArray<FMissionDef> C;

		FMissionDef M0;
		M0.Name = TEXT("First Contact");
		M0.Enemies = { EEnemyType::Scout, EEnemyType::Gunship };
		{
			FCommsBeat B; B.Sender = TEXT("COMMAND"); B.AtSeconds = 1.0f;
			B.Text = TEXT("Bridge, two unknowns just dropped in. Identify and engage."); M0.Comms.Add(B);
		}
		{
			FCommsBeat B; B.Sender = TEXT("SCOUT-7"); B.OnKill = 1;
			B.Text = TEXT("They're hostile — the scout is down. Watch the gunship's beam."); M0.Comms.Add(B);
		}
		C.Add(M0);

		FMissionDef M1;
		M1.Name = TEXT("Patrol Ambush");
		M1.Enemies = { EEnemyType::Gunship, EEnemyType::Gunship, EEnemyType::Cruiser };
		{
			FCommsBeat B; B.Sender = TEXT("COMMAND"); B.AtSeconds = 1.0f;
			B.Text = TEXT("It was a trap — a cruiser escort just lit up. Hold the line."); M1.Comms.Add(B);
		}
		{
			FCommsBeat B; B.Sender = TEXT("ENGINEER"); B.OnKill = 2;
			B.Text = TEXT("Escorts clear. The cruiser's shields are heavy — torpedoes will punch through."); M1.Comms.Add(B);
		}
		C.Add(M1);

		FMissionDef M2;
		M2.Name = TEXT("Last Stand");
		M2.Enemies = { EEnemyType::Scout, EEnemyType::Scout, EEnemyType::Gunship, EEnemyType::Cruiser };
		{
			FCommsBeat B; B.Sender = TEXT("COMMAND"); B.AtSeconds = 1.0f;
			B.Text = TEXT("This is the last of them. Clear this sector and we're done. Good luck."); M2.Comms.Add(B);
		}
		{
			FCommsBeat B; B.Sender = TEXT("COMMAND"); B.OnKill = 3;
			B.Text = TEXT("Just the cruiser left. Finish it, Captain."); M2.Comms.Add(B);
		}
		C.Add(M2);

		return C;
	}
}

int32 UMissionSubsystem::MissionCount()
{
	static const int32 Count = BuildCampaign().Num();
	return Count;
}

FMissionDef UMissionSubsystem::GetMissionDef(int32 Index)
{
	static const TArray<FMissionDef> Campaign = BuildCampaign();
	const int32 Clamped = FMath::Clamp(Index, 0, Campaign.Num() - 1);
	return Campaign[Clamped];
}

bool UMissionSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UMissionSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (const USpaceGameInstance* GI = InWorld.GetGameInstance<USpaceGameInstance>())
	{
		MissionIndex = GI->GetMissionIndex();
	}
	Mission = GetMissionDef(MissionIndex);

	UE_LOG(LogTemp, Log, TEXT("[Mission] Building mission %d '%s' — %d hostile(s)"),
		MissionIndex, *Mission.Name, Mission.Enemies.Num());

	BuildEncounter(InWorld);
}

void UMissionSubsystem::BuildEncounter(UWorld& World)
{
	// Anchor the fleet on the player: spawn in a fan ahead of the ship's nose.
	FVector Anchor = FVector::ZeroVector;
	FVector Forward = FVector::ForwardVector;
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(&World, 0))
	{
		Anchor = Player->GetActorLocation();
		Forward = Player->GetActorForwardVector();
	}

	// Clear any level-placed hostiles so the mission owns the encounter (non-destructive to the map).
	TArray<AActor*> Placed;
	UGameplayStatics::GetAllActorsOfClass(&World, AEnemyShip::StaticClass(), Placed);
	for (AActor* A : Placed) { if (A) { A->Destroy(); } }

	const int32 N = Mission.Enemies.Num();
	for (int32 i = 0; i < N; ++i)
	{
		// Spread across ±35° and stagger the range so they don't stack.
		const float T = (N <= 1) ? 0.f : ((float)i / (float)(N - 1) - 0.5f);
		const float AngleDeg = T * 70.f;
		const float Dist = 9000.f + (i % 2) * 2600.f;
		const FVector Dir = Forward.RotateAngleAxis(AngleDeg, FVector::UpVector);
		const FVector Loc = Anchor + Dir * Dist;
		const FRotator Rot = (Anchor - Loc).Rotation(); // face back toward the player

		FTransform Xform(Rot, Loc);
		AEnemyShip* Enemy = World.SpawnActorDeferred<AEnemyShip>(
			AEnemyShip::StaticClass(), Xform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Enemy)
		{
			Enemy->ShipType = Mission.Enemies[i];
			UGameplayStatics::FinishSpawningActor(Enemy, Xform);
		}
	}

	// Subsystems begin play after the player controller, so its win-condition binding ran before
	// these ships existed — re-bind now that the fleet is spawned.
	if (ABridgePlayerController* PC = Cast<ABridgePlayerController>(
		UGameplayStatics::GetPlayerController(&World, 0)))
	{
		PC->BindEnemyDeaths();
	}
}
