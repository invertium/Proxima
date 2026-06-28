// SpaceGame — bridge simulator. Mission/encounter builder implementation.

#include "Core/MissionSubsystem.h"

#include "Components/HealthComponent.h"
#include "Core/BridgePlayerController.h"
#include "Core/SpaceGameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Ships/EnemyShip.h"
#include "TimerManager.h"
#include "World/Station.h"

namespace
{
	// The campaign: escalating fleets built from the three enemy archetypes, each with a short
	// comms script (surfaced on the Science console in M18.6). Keep the size in sync with
	// USpaceGameInstance::GetMissionCount().
	TArray<FMissionDef> BuildCampaign()
	{
		TArray<FMissionDef> C;

		// Mission 0 — Shakedown: the story-mode tutorial. A narrated walk through the four stations,
		// then a single passive target drone (EngageDelayOverride huge → never fires) to drill weapons.
		FMissionDef Tut;
		Tut.Name = TEXT("Shakedown Cruise");
		Tut.Enemies = { EEnemyType::Scout };
		Tut.EngageDelayOverride = 100000.f; // the drone holds fire all mission — a safe target
		{
			FCommsBeat B; B.Sender = TEXT("CMDR VOSS"); B.AtSeconds = 1.5f;
			B.Text = TEXT("Welcome to the bridge, Captain. Before real orders, a shakedown. HELM (console 1): ease the throttle up and bring her around."); Tut.Comms.Add(B);
		}
		{
			FCommsBeat B; B.Sender = TEXT("ENGINEER KANE"); B.AtSeconds = 11.f;
			B.Text = TEXT("ENGINEERING (console 3): reactor power is a balance — feed one system and another starves. Give it a try, then weld any hull damage."); Tut.Comms.Add(B);
		}
		{
			FCommsBeat B; B.Sender = TEXT("CMDR VOSS"); B.AtSeconds = 22.f;
			B.Text = TEXT("See the STARBASE on your scope? Fly back and DOCK to repair, rearm, and outfit the ship at the drydock. You'll lean on it out there."); Tut.Comms.Add(B);
		}
		{
			FCommsBeat B; B.Sender = TEXT("TACTICAL"); B.AtSeconds = 33.f;
			B.Text = TEXT("Sensors tag a derelict raider — reactor cold, no threat. WEAPONS (console 2): lock it, point your bow to bring it into the firing arc, and fire."); Tut.Comms.Add(B);
		}
		{
			FCommsBeat B; B.Sender = TEXT("CMDR VOSS"); B.OnKill = 1;
			B.Text = TEXT("Clean kill. You're cleared for active duty, Captain — real orders are inbound. Stay sharp out there."); Tut.Comms.Add(B);
		}
		C.Add(Tut);

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

	// Run the mission's comms script: timed beats off a repeating timer (paused with the game),
	// kill-triggered beats off the hostiles' deaths (bound in BuildEncounter).
	CommsLog.Reset();
	KillCount = 0;
	StartTime = InWorld.GetTimeSeconds();
	InWorld.GetTimerManager().SetTimer(BeatTimer, this, &UMissionSubsystem::CheckTimedBeats, 0.25f, true);
}

void UMissionSubsystem::CheckTimedBeats()
{
	const UWorld* World = GetWorld();
	if (!World) { return; }
	const float Elapsed = World->GetTimeSeconds() - StartTime;
	for (FCommsBeat& B : Mission.Comms)
	{
		if (!B.bFired && B.OnKill < 0 && B.AtSeconds <= Elapsed)
		{
			FireBeat(B);
		}
	}
}

void UMissionSubsystem::HandleEnemyKilled(AActor* DeadActor)
{
	++KillCount;
	for (FCommsBeat& B : Mission.Comms)
	{
		if (!B.bFired && B.OnKill == KillCount)
		{
			FireBeat(B);
		}
	}
}

void UMissionSubsystem::FireBeat(FCommsBeat& Beat)
{
	Beat.bFired = true;
	FCommsMessage Msg;
	Msg.Sender = Beat.Sender;
	Msg.Text = Beat.Text;
	CommsLog.Add(Msg);
	UE_LOG(LogTemp, Log, TEXT("[Comms] %s: %s"), *Beat.Sender, *Beat.Text);
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

	// Home starbase (M19): a friendly dock just behind the player's start. The crew flies back to
	// it to repair, restock, and spend salvage at the Engineering drydock. Spawn one per encounter.
	{
		TArray<AActor*> ExistingStations;
		UGameplayStatics::GetAllActorsOfClass(&World, AStation::StaticClass(), ExistingStations);
		if (ExistingStations.Num() == 0)
		{
			const FVector StationLoc = Anchor - Forward * 5000.f;
			const FRotator StationRot = Forward.Rotation(); // face the same way as the player
			World.SpawnActor<AStation>(AStation::StaticClass(), StationLoc, StationRot);
		}
	}

	const int32 N = Mission.Enemies.Num();
	TMap<EEnemyType, int32> TypeCounts; // per-archetype ordinal, so callsigns read WASP-1, HORNET-2, ...
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
			const EEnemyType Type = Mission.Enemies[i];
			Enemy->ShipType = Type;
			Enemy->Callsign = AEnemyShip::MakeCallsign(Type, TypeCounts.FindOrAdd(Type)++);
			if (Mission.EngageDelayOverride >= 0.f)
			{
				Enemy->SetEngageDelay(Mission.EngageDelayOverride); // passive target drone (tutorial)
			}
			UGameplayStatics::FinishSpawningActor(Enemy, Xform);
			// Drive kill-triggered comms beats off each hostile's death.
			if (UHealthComponent* H = Enemy->GetHealthComp())
			{
				H->OnDeath.AddUniqueDynamic(this, &UMissionSubsystem::HandleEnemyKilled);
			}
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
