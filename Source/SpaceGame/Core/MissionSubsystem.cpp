// SpaceGame — bridge simulator. Mission/encounter builder implementation.

#include "Core/MissionSubsystem.h"

#include "Components/HealthComponent.h"
#include "Core/BridgePlayerController.h"
#include "Core/SpaceGameInstance.h"
#include "Core/SpaceGameMode.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Ships/EnemyShip.h"
#include "TimerManager.h"
#include "World/Station.h"
#include "World/WorldLandmark.h"

namespace
{
	// How far the normalised (0..1) sector map stretches across world space, in uu. Systems are
	// placed by their MapX/MapY relative to the home system, so warp meaningfully shortens the hops.
	constexpr float SectorSpan = 120000.f;

	// Shift the whole sector so the home body sits clear of the player's start (the ship begins in
	// open space beside Haven, not embedded in it). Applied to every system's world position.
	const FVector HomeOffset(0.f, 14000.f, 0.f);
}

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
		Tut.bAutoLaunch = true;             // tutorial skips staging: the (passive) drone is up at once
		Tut.MapX = 0.16f; Tut.MapY = 0.52f; // home space, near the friendly core of the sector
		Tut.LandmarkName = TEXT("Haven"); Tut.LandmarkKind = ELandmarkKind::Planet;
		Tut.LandmarkColor = FLinearColor(0.35f, 0.6f, 1.0f, 1.f); Tut.LandmarkScale = 1.0f;
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

		// --- Story arc: the Crimson Pact incursion into the Veil frontier ---

		FMissionDef M0;
		M0.Name = TEXT("First Contact");
		M0.Enemies = { EEnemyType::Scout, EEnemyType::Gunship };
		M0.BriefSender = TEXT("CMDR VOSS");
		M0.BriefText = TEXT("Captain — long-range sensors caught raiders probing the Veil frontier: a scout and a gunship squatting on the trade lane. Identify them and drive them off. We need to know who we're dealing with.");
		M0.MapX = 0.40f; M0.MapY = 0.36f; // the trade lane, out from home
		M0.LandmarkName = TEXT("Tarsis"); M0.LandmarkKind = ELandmarkKind::Planet;
		M0.LandmarkColor = FLinearColor(0.3f, 0.95f, 0.85f, 1.f); M0.LandmarkScale = 0.9f;
		{
			FCommsBeat B; B.Sender = TEXT("TACTICAL"); B.AtSeconds = 1.5f;
			B.Text = TEXT("Contacts confirmed — those are Crimson Pact markings. They're not answering hails. Weapons free."); M0.Comms.Add(B);
		}
		{
			FCommsBeat B; B.Sender = TEXT("CMDR VOSS"); B.OnKill = 1;
			B.Text = TEXT("Scout's down — but it got a transmission off before it died. The gunship's the real threat now; mind its beam."); M0.Comms.Add(B);
		}
		C.Add(M0);

		FMissionDef M1;
		M1.Name = TEXT("Patrol Ambush");
		M1.Enemies = { EEnemyType::Gunship, EEnemyType::Gunship, EEnemyType::Cruiser };
		M1.BriefSender = TEXT("CMDR VOSS");
		M1.BriefText = TEXT("That first contact wasn't a probe — it was bait, and you took it. A Pact patrol with a cruiser escort is waiting where that scout's signal led. Go in heavy, Captain.");
		M1.MapX = 0.62f; M1.MapY = 0.60f; // deeper into contested space
		M1.LandmarkName = TEXT("Korrin Belt"); M1.LandmarkKind = ELandmarkKind::Planet;
		M1.LandmarkColor = FLinearColor(1.0f, 0.55f, 0.2f, 1.f); M1.LandmarkScale = 1.1f;
		{
			FCommsBeat B; B.Sender = TEXT("TACTICAL"); B.AtSeconds = 1.5f;
			B.Text = TEXT("Ambush! Two gunships and a cruiser just powered up around us. Should've seen it coming."); M1.Comms.Add(B);
		}
		{
			FCommsBeat B; B.Sender = TEXT("ENGINEER KANE"); B.OnKill = 2;
			B.Text = TEXT("Escorts are scrap. That cruiser's shields are layered thick — route power to weapons and let the torpedoes punch through."); M1.Comms.Add(B);
		}
		C.Add(M1);

		FMissionDef M2;
		M2.Name = TEXT("Warlord's Reach");
		M2.Enemies = { EEnemyType::Scout, EEnemyType::Scout, EEnemyType::Gunship, EEnemyType::Cruiser };
		M2.BriefSender = TEXT("CMDR VOSS");
		M2.BriefText = TEXT("We back-traced the patrol to the Pact's staging point. Their warlord's flagship is here with everything she has left. Break this fleet and the Crimson Pact is finished in the Veil. This is the one that matters.");
		M2.MapX = 0.86f; M2.MapY = 0.40f; // the Pact staging point, edge of the sector
		M2.LandmarkName = TEXT("Ember"); M2.LandmarkKind = ELandmarkKind::Sun; // a star — the Pact's forge
		M2.LandmarkColor = FLinearColor(1.0f, 0.7f, 0.2f, 1.f); M2.LandmarkScale = 1.0f;
		{
			FCommsBeat B; B.Sender = TEXT("CMDR VOSS"); B.AtSeconds = 1.5f;
			B.Text = TEXT("All hands, battle stations. Whatever happens out there — it's been an honour flying with this crew. For the frontier."); M2.Comms.Add(B);
		}
		{
			FCommsBeat B; B.Sender = TEXT("TACTICAL"); B.OnKill = 3;
			B.Text = TEXT("Screen's down — just the flagship left. She's wounded and she knows it. Finish it, Captain."); M2.Comms.Add(B);
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

	// Front-end worlds (MainMenu) are the same Game world type but run AMenuGameMode — don't
	// build the sector behind the menu (no starbase/landmarks/director until the bridge loads).
	if (!InWorld.GetAuthGameMode<ASpaceGameMode>())
	{
		return;
	}

	if (const USpaceGameInstance* GI = InWorld.GetGameInstance<USpaceGameInstance>())
	{
		MissionIndex = GI->GetMissionIndex();
	}

	CommsLog.Reset();
	bEncounterLive = false;
	bSectorComplete = false;
	LiveFleet.Reset();

	// Clear any level-placed hostiles up front so the open sector starts genuinely empty — each
	// objective's fleet is spawned on proximity. Non-destructive to the saved map.
	TArray<AActor*> Placed;
	UGameplayStatics::GetAllActorsOfClass(&InWorld, AEnemyShip::StaticClass(), Placed);
	for (AActor* A : Placed) { if (A) { A->Destroy(); } }

	// Sector anchor = the player's start; the home system + starbase sit here, others spread out.
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(&InWorld, 0))
	{
		SectorAnchor = Player->GetActorLocation();
	}

	// The starbase + every system's landmark are present from the start — the crew flies the open sector.
	SpawnStation(InWorld);
	SpawnLandmarks(InWorld);

	// Arm the active objective (fire its briefing) and start the director watching for proximity.
	BeginObjective(MissionIndex);
	InWorld.GetTimerManager().SetTimer(DirectorTimer, this, &UMissionSubsystem::CheckDirector, 0.25f, true);
	UE_LOG(LogTemp, Log, TEXT("[Sector] Director online — objective %d '%s' at '%s'"),
		MissionIndex, *Mission.Name, *Mission.LandmarkName);
}

void UMissionSubsystem::BeginObjective(int32 Index)
{
	MissionIndex = Index;
	Mission = GetMissionDef(Index);   // a fresh copy, so its comms beats start un-fired.
	KillCount = 0;
	bEncounterLive = false;
	LiveFleet.Reset();

	// Fire the objective's briefing (combat missions carry orders; the tutorial's timed comms cover it).
	if (!Mission.BriefText.IsEmpty())
	{
		FCommsMessage Brief;
		Brief.Sender = Mission.BriefSender.IsEmpty() ? TEXT("CMDR VOSS") : Mission.BriefSender;
		Brief.Text = Mission.BriefText;
		if (!Mission.LandmarkName.IsEmpty())
		{
			Brief.Text += FString::Printf(TEXT("  Set course for %s and engage."), *Mission.LandmarkName);
		}
		CommsLog.Add(Brief);
	}
	UE_LOG(LogTemp, Log, TEXT("[Sector] Objective armed: %d '%s' (fly to '%s')"),
		Index, *Mission.Name, *Mission.LandmarkName);
}

void UMissionSubsystem::CheckDirector()
{
	if (bEncounterLive || bSectorComplete) { return; }
	const UWorld* World = GetWorld();
	if (!World) { return; }

	const APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0);
	if (!Player) { return; }

	// Planar range to the active objective's landmark — enter the zone and the fleet powers up.
	FVector Delta = Player->GetActorLocation() - GetActiveObjectiveLocation();
	Delta.Z = 0.f;
	if (Delta.SizeSquared() <= TriggerRadius * TriggerRadius)
	{
		TriggerActiveEncounter();
	}
}

void UMissionSubsystem::TriggerActiveEncounter()
{
	if (bEncounterLive) { return; }
	UWorld* World = GetWorld();
	if (!World) { return; }

	bEncounterLive = true;
	SpawnFleet(*World);

	// Time the mission's comms beats from the moment the fight opens.
	StartTime = World->GetTimeSeconds();
	World->GetTimerManager().SetTimer(BeatTimer, this, &UMissionSubsystem::CheckTimedBeats, 0.25f, true);
	UE_LOG(LogTemp, Log, TEXT("[Sector] ENCOUNTER at '%s' — %d hostile(s) inbound"),
		*Mission.LandmarkName, Mission.Enemies.Num());
}

void UMissionSubsystem::ForceTriggerObjective()
{
	TriggerActiveEncounter();
}

void UMissionSubsystem::OnFleetCleared()
{
	UWorld* World = GetWorld();
	bEncounterLive = false;
	LiveFleet.Reset();
	if (World) { World->GetTimerManager().ClearTimer(BeatTimer); }

	const bool bWasFinal = MissionIndex >= MissionCount() - 1;

	// Advance + persist the campaign so the next objective is hot (and a retry resumes here).
	if (USpaceGameInstance* GI = World ? World->GetGameInstance<USpaceGameInstance>() : nullptr)
	{
		GI->AdvanceMission();
		GI->SaveCampaign();
	}

	if (bWasFinal)
	{
		bSectorComplete = true;
		UE_LOG(LogTemp, Log, TEXT("[Sector] FINAL objective cleared — campaign complete"));
		// Hand off to the controller for the campaign epilogue outcome screen.
		if (ABridgePlayerController* PC = Cast<ABridgePlayerController>(
			UGameplayStatics::GetPlayerController(World, 0)))
		{
			PC->OnCampaignComplete();
		}
		return;
	}

	// Seamless clear: a short comms beat, then arm the next objective (no reload, no overlay).
	const FMissionDef Next = GetMissionDef(MissionIndex + 1);
	FCommsMessage Clear;
	Clear.Sender = TEXT("TACTICAL");
	Clear.Text = FString::Printf(
		TEXT("Sector cleared, Captain. Next objective: %s. Lay in a course."),
		Next.LandmarkName.IsEmpty() ? *Next.Name : *Next.LandmarkName);
	CommsLog.Add(Clear);
	UE_LOG(LogTemp, Log, TEXT("[Sector] Cleared '%s' — advancing to '%s'"), *Mission.Name, *Next.Name);

	BeginObjective(MissionIndex + 1);
}

int32 UMissionSubsystem::CountLiveFleet() const
{
	int32 Alive = 0;
	for (const TWeakObjectPtr<AEnemyShip>& Ptr : LiveFleet)
	{
		const AEnemyShip* Enemy = Ptr.Get();
		const UHealthComponent* Health = Enemy ? Enemy->GetHealthComp() : nullptr;
		if (Health && Health->IsAlive()) { ++Alive; }
	}
	return Alive;
}

FString UMissionSubsystem::GetObjectiveName() const
{
	return Mission.LandmarkName.IsEmpty() ? Mission.Name : Mission.LandmarkName;
}

float UMissionSubsystem::GetObjectiveDistance() const
{
	const UWorld* World = GetWorld();
	const APawn* Player = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
	if (!Player) { return -1.f; }
	FVector Delta = Player->GetActorLocation() - GetActiveObjectiveLocation();
	Delta.Z = 0.f;
	return Delta.Size();
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

	// The just-killed ship reads hull 0 (not alive); when the whole fleet is down, the sector clears.
	if (bEncounterLive && CountLiveFleet() == 0)
	{
		OnFleetCleared();
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

FVector UMissionSubsystem::GetSystemLocation(int32 Index) const
{
	const FMissionDef Home = GetMissionDef(0);
	const FMissionDef Def = GetMissionDef(Index);
	const float Dx = (Def.MapX - Home.MapX) * SectorSpan;
	const float Dy = (Def.MapY - Home.MapY) * SectorSpan;
	return SectorAnchor + HomeOffset + FVector(Dx, Dy, 0.f);
}

FVector2D UMissionSubsystem::GetMapPosition(FVector World) const
{
	// Inverse of GetSystemLocation: fold a world XY back into the normalised (0..1) sector-map space
	// so the nav map can plot the live ship alongside the systems.
	const FMissionDef Home = GetMissionDef(0);
	const float MapX = Home.MapX + (World.X - SectorAnchor.X - HomeOffset.X) / SectorSpan;
	const float MapY = Home.MapY + (World.Y - SectorAnchor.Y - HomeOffset.Y) / SectorSpan;
	return FVector2D(MapX, MapY);
}

void UMissionSubsystem::SpawnLandmarks(UWorld& World)
{
	// One celestial body per system, spread across the sector (skip if already present on reload).
	TArray<AActor*> Existing;
	UGameplayStatics::GetAllActorsOfClass(&World, AWorldLandmark::StaticClass(), Existing);
	if (Existing.Num() > 0) { return; }

	for (int32 i = 0; i < MissionCount(); ++i)
	{
		const FMissionDef Def = GetMissionDef(i);
		const FVector Loc = GetSystemLocation(i);
		if (AWorldLandmark* L = World.SpawnActor<AWorldLandmark>(AWorldLandmark::StaticClass(), Loc, FRotator::ZeroRotator))
		{
			L->Setup(Def.LandmarkName, Def.LandmarkScale, Def.LandmarkColor, Def.LandmarkKind == ELandmarkKind::Sun);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[Sector] Spawned %d system landmarks (span %.0f uu)"), MissionCount(), SectorSpan);
}

void UMissionSubsystem::SpawnStation(UWorld& World)
{
	// Home starbase (M19): a friendly dock just behind the player's start. The crew flies back to
	// it to repair, restock, and spend salvage at the Engineering drydock. Spawn one per encounter.
	FVector Anchor = FVector::ZeroVector;
	FVector Forward = FVector::ForwardVector;
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(&World, 0))
	{
		Anchor = Player->GetActorLocation();
		Forward = Player->GetActorForwardVector();
	}

	TArray<AActor*> ExistingStations;
	UGameplayStatics::GetAllActorsOfClass(&World, AStation::StaticClass(), ExistingStations);
	if (ExistingStations.Num() == 0)
	{
		const FVector StationLoc = Anchor - Forward * 5000.f;
		const FRotator StationRot = Forward.Rotation(); // face the same way as the player
		World.SpawnActor<AStation>(AStation::StaticClass(), StationLoc, StationRot);
	}
}

void UMissionSubsystem::SpawnFleet(UWorld& World)
{
	// Open sector (M23): the fleet powers up *around its landmark*, clustered on the side the player
	// is arriving from and facing the incoming ship.
	const FVector Center = GetActiveObjectiveLocation();
	FVector PlayerLoc = Center + FVector::ForwardVector * 12000.f;
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(&World, 0))
	{
		PlayerLoc = Player->GetActorLocation();
	}
	FVector ToPlayer = PlayerLoc - Center;
	ToPlayer.Z = 0.f;
	const FVector FaceDir = ToPlayer.IsNearlyZero() ? FVector::ForwardVector : ToPlayer.GetSafeNormal();

	LiveFleet.Reset();
	const int32 N = Mission.Enemies.Num();
	TMap<EEnemyType, int32> TypeCounts; // per-archetype ordinal, so callsigns read WASP-1, HORNET-2, ...
	for (int32 i = 0; i < N; ++i)
	{
		// Fan across ±35° on the player-facing side of the landmark, staggering range so they don't stack.
		const float T = (N <= 1) ? 0.f : ((float)i / (float)(N - 1) - 0.5f);
		const float AngleDeg = T * 70.f;
		const float Dist = 6000.f + (i % 2) * 2600.f;
		const FVector Dir = FaceDir.RotateAngleAxis(AngleDeg, FVector::UpVector);
		const FVector Loc = Center + Dir * Dist;
		const FRotator Rot = (PlayerLoc - Loc).Rotation(); // face the incoming player

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
			LiveFleet.Add(Enemy);
			// Drive kill-triggered comms beats + the clear check off each hostile's death.
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
