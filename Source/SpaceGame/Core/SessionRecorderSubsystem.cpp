// SpaceGame — bridge simulator. Session telemetry recorder (dev/analysis "record mode").

#include "Core/SessionRecorderSubsystem.h"

#include "Core/MissionSubsystem.h"
#include "Core/SpaceGameInstance.h"
#include "Core/SpaceGameMode.h"
#include "Ships/EnemyShip.h"
#include "Ships/Spaceship.h"
#include "Components/HealthComponent.h"
#include "Components/ShipMovementComponent.h"
#include "Components/TorpedoLauncherComponent.h"
#include "Components/WeaponComponent.h"

#include "EngineUtils.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DateTime.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

// The mode toggle: 0 = off (default), 1 = record. Settable live from the console (`sg.RecordSession 1`)
// so a session can be captured on demand without a rebuild.
static TAutoConsoleVariable<int32> CVarRecordSession(
	TEXT("sg.RecordSession"), 0,
	TEXT("Record gameplay telemetry to Saved/SessionLogs/*.jsonl for offline analysis (0=off, 1=on)."),
	ECVF_Default);

// Sample cadence. 0.5 s (2 Hz) is plenty to reconstruct movement/combat while staying tiny on disk.
static TAutoConsoleVariable<float> CVarRecordInterval(
	TEXT("sg.RecordSampleInterval"), 0.5f,
	TEXT("Seconds between recorded telemetry samples (default 0.5 = 2 Hz)."),
	ECVF_Default);

namespace
{
	FString JsonEsc(const FString& In)
	{
		FString Out = In;
		Out.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
		Out.ReplaceInline(TEXT("\""), TEXT("\\\""));
		return Out;
	}
}

bool USessionRecorderSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void USessionRecorderSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Let `-recordsession` on the command line arm the mode from launch (headless / packaged capture).
	if (FParse::Param(FCommandLine::Get(), TEXT("recordsession")))
	{
		CVarRecordSession->Set(1, ECVF_SetByCommandline);
	}
}

void USessionRecorderSubsystem::Deinitialize()
{
	StopRecording(TEXT("worldend"));
	Super::Deinitialize();
}

TStatId USessionRecorderSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USessionRecorderSubsystem, STATGROUP_Tickables);
}

ASpaceship* USessionRecorderSubsystem::GetShip() const
{
	if (const UWorld* World = GetWorld())
	{
		return Cast<ASpaceship>(UGameplayStatics::GetPlayerPawn(World, 0));
	}
	return nullptr;
}

int32 USessionRecorderSubsystem::GetPhaseCode() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const ASpaceGameMode* GM = World->GetAuthGameMode<ASpaceGameMode>())
		{
			switch (GM->GetPhase())
			{
			case EGamePhase::Victory: return 1;
			case EGamePhase::Defeat:  return 2;
			default:                  return 0;
			}
		}
	}
	return 0;
}

int32 USessionRecorderSubsystem::CountLiveEnemies() const
{
	int32 Count = 0;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AEnemyShip> It(World); It; ++It)
		{
			const UHealthComponent* H = It->GetHealthComp();
			if (H && H->IsAlive()) { ++Count; }
		}
	}
	return Count;
}

void USessionRecorderSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UWorld* World = GetWorld();
	if (!World || !World->HasBegunPlay()) { return; }

	const bool bWant = CVarRecordSession.GetValueOnGameThread() != 0;
	if (!bWant)
	{
		if (IsRecording()) { StopRecording(TEXT("disabled")); }
		return;
	}

	// Wait for a player ship to exist (so the menu map / pre-spawn frames make no file).
	ASpaceship* Ship = GetShip();
	if (!Ship) { return; }

	if (!IsRecording())
	{
		StartRecording(Ship);
		if (!IsRecording()) { return; } // open failed — don't spin
	}

	const float Interval = FMath::Max(0.05f, CVarRecordInterval.GetValueOnGameThread());
	SampleAccum += DeltaTime;
	if (SampleAccum >= Interval)
	{
		SampleAccum = 0.f;
		RecordSample(Ship);
	}
}

void USessionRecorderSubsystem::StartRecording(const ASpaceship* Ship)
{
	const FDateTime Now = FDateTime::Now();
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("SessionLogs");
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	PF.CreateDirectoryTree(*Dir);

	FilePath = Dir / FString::Printf(TEXT("session_%s.jsonl"), *Now.ToString(TEXT("%Y%m%d_%H%M%S")));
	FileHandle.Reset(PF.OpenWrite(*FilePath, /*bAppend*/ false, /*bAllowRead*/ true));
	if (!FileHandle)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SessionRecorder] Could not open %s for writing"), *FilePath);
		return;
	}

	StartUnixTime = static_cast<double>(Now.ToUnixTimestamp());
	const UWorld* World = GetWorld();
	StartWorldTime = World ? World->GetTimeSeconds() : 0.f;
	SampleAccum = 0.f;
	Prev = FSnapshot();

	const USpaceGameInstance* GI = World ? World->GetGameInstance<USpaceGameInstance>() : nullptr;
	FString Ver;
	GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), Ver, GGameIni);
	const float Interval = FMath::Max(0.05f, CVarRecordInterval.GetValueOnGameThread());

	WriteLine(FString::Printf(TEXT(
		"{\"k\":\"meta\",\"unix\":%lld,\"ver\":\"%s\",\"map\":\"%s\","
		"\"difficulty\":%d,\"ship\":%d,\"mission\":%d,\"sampleHz\":%.2f}"),
		static_cast<long long>(StartUnixTime), *JsonEsc(Ver),
		*JsonEsc(World ? World->GetMapName() : TEXT("?")),
		GI ? static_cast<int32>(GI->GetDifficulty()) : 0,
		GI ? static_cast<int32>(GI->GetPlayerShip()) : 0,
		GI ? GI->GetMissionIndex() : 0,
		1.f / Interval));

	// Capture a t=0 sample immediately so the track starts at spawn.
	RecordSample(Ship);

	UE_LOG(LogTemp, Log, TEXT("[SessionRecorder] Recording to %s"), *FilePath);
}

void USessionRecorderSubsystem::StopRecording(const TCHAR* Reason)
{
	if (!FileHandle) { return; }
	const UWorld* World = GetWorld();
	const float Dur = (World ? World->GetTimeSeconds() : 0.f) - StartWorldTime;
	WriteLine(FString::Printf(TEXT("{\"k\":\"end\",\"t\":%.2f,\"reason\":\"%s\"}"), Dur, Reason));
	FileHandle.Reset();
	UE_LOG(LogTemp, Log, TEXT("[SessionRecorder] Closed %s (%.1fs)"), *FilePath, Dur);
}

void USessionRecorderSubsystem::WriteLine(const FString& JsonObject)
{
	if (!FileHandle) { return; }
	const FString Line = JsonObject + TEXT("\n");
	const FTCHARToUTF8 Utf8(*Line);
	FileHandle->Write(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	FileHandle->Flush(); // per-line flush: a crash still leaves everything up to the last sample.
}

void USessionRecorderSubsystem::RecordSample(const ASpaceship* Ship)
{
	if (!Ship || !FileHandle) { return; }
	UWorld* World = GetWorld();
	const float T = (World ? World->GetTimeSeconds() : 0.f) - StartWorldTime;

	const UShipMovementComponent* Move = Ship->GetMovementComp();
	const UWeaponComponent* Weap = Ship->GetWeaponComp();
	const UHealthComponent* Health = Ship->GetHealthComp();
	const UTorpedoLauncherComponent* Torp = Ship->GetTorpedoComp();

	const float Hull = Health ? Health->GetHull() : 0.f;
	const float MaxHull = Health ? Health->GetMaxHull() : 0.f;
	const float Shield = Health ? Health->GetShield() : 0.f;
	const float MaxShield = Health ? Health->GetMaxShield() : 0.f;
	const FVector Loc = Ship->GetActorLocation();
	const float Hdg = Ship->GetActorRotation().Yaw;
	const float Spd = Move ? Move->GetSpeed() : 0.f;
	const float Thr = Move ? Move->GetThrottle() : 0.f;
	const int32 Ammo = Torp ? Torp->GetAmmo() : 0;
	const float WarpC = Ship->GetWarpCharge();
	const bool bDocked = Ship->IsDocked();
	const bool bRed = Ship->IsRedAlert();

	const AActor* TargetActor = Weap ? Weap->GetCurrentTarget() : nullptr;
	FString TargetName = TEXT("none");
	if (const AEnemyShip* E = Cast<AEnemyShip>(TargetActor)) { TargetName = E->GetCallsign(); }
	const float TRange = Weap ? Weap->GetTargetRange() : -1.f;

	int32 MissionIndex = 0;
	FString MissionName, Objective;
	bool bEngaged = false;
	float ObjDist = -1.f;
	int32 Wave = 0;
	if (World)
	{
		if (const UMissionSubsystem* MS = World->GetSubsystem<UMissionSubsystem>())
		{
			MissionIndex = MS->GetMissionIndex();
			MissionName = MS->GetMissionName();
			Objective = MS->GetObjectiveName();
			bEngaged = MS->IsEncounterLive();
			ObjDist = MS->GetObjectiveDistance();
			Wave = MS->GetSkirmishWave();
		}
	}

	const USpaceGameInstance* GI = World ? World->GetGameInstance<USpaceGameInstance>() : nullptr;
	const int32 Credits = GI ? GI->GetCredits() : 0;
	const int32 XP = GI ? GI->GetXP() : 0;
	const int32 Rank = GI ? GI->GetRank() : 1;

	const int32 Enemies = CountLiveEnemies();
	const int32 Phase = GetPhaseCode();

	// --- discrete events, derived by diffing against the previous sample ---
	if (Prev.bValid)
	{
		auto Event = [&](const FString& Body)
		{
			WriteLine(FString::Printf(TEXT("{\"k\":\"e\",\"t\":%.2f,%s}"), T, *Body));
		};

		const float HullLost = Prev.Hull - Hull;
		if (HullLost > 0.5f)
		{
			Event(FString::Printf(TEXT("\"type\":\"damage\",\"amount\":%.1f,\"hull\":%.1f"), HullLost, Hull));
		}
		if (Hull <= 0.f && Prev.Hull > 0.f) { Event(TEXT("\"type\":\"death\"")); }
		if (Enemies < Prev.Enemies)
		{
			Event(FString::Printf(TEXT("\"type\":\"kill\",\"n\":%d,\"remaining\":%d"), Prev.Enemies - Enemies, Enemies));
		}
		if (Enemies > Prev.Enemies && Prev.Enemies == 0)
		{
			Event(FString::Printf(TEXT("\"type\":\"engage\",\"fleet\":%d"), Enemies));
		}
		if (MissionIndex != Prev.MissionIndex)
		{
			Event(FString::Printf(TEXT("\"type\":\"mission\",\"index\":%d,\"name\":\"%s\""), MissionIndex, *JsonEsc(MissionName)));
		}
		if (Phase != Prev.Phase && Phase != 0)
		{
			Event(FString::Printf(TEXT("\"type\":\"%s\""), Phase == 1 ? TEXT("victory") : TEXT("defeat")));
		}
		if (bDocked && !Prev.bDocked) { Event(TEXT("\"type\":\"dock\"")); }
		if (!bDocked && Prev.bDocked) { Event(TEXT("\"type\":\"undock\"")); }
		if (Prev.WarpCharge - WarpC > 0.4f) { Event(TEXT("\"type\":\"warp\"")); }
	}

	// --- the periodic state sample ---
	WriteLine(FString::Printf(TEXT(
		"{\"k\":\"s\",\"t\":%.2f,\"phase\":%d,"
		"\"px\":%.0f,\"py\":%.0f,\"pz\":%.0f,\"hdg\":%.1f,\"spd\":%.1f,\"thr\":%.2f,"
		"\"hull\":%.1f,\"maxHull\":%.1f,\"shield\":%.1f,\"maxShield\":%.1f,"
		"\"red\":%d,\"docked\":%d,\"warp\":%.3f,\"ammo\":%d,"
		"\"target\":\"%s\",\"tRange\":%.0f,"
		"\"mission\":%d,\"missionName\":\"%s\",\"objective\":\"%s\",\"engaged\":%d,\"objDist\":%.0f,\"wave\":%d,"
		"\"enemies\":%d,\"credits\":%d,\"xp\":%d,\"rank\":%d}"),
		T, Phase,
		Loc.X, Loc.Y, Loc.Z, Hdg, Spd, Thr,
		Hull, MaxHull, Shield, MaxShield,
		bRed ? 1 : 0, bDocked ? 1 : 0, WarpC, Ammo,
		*JsonEsc(TargetName), TRange,
		MissionIndex, *JsonEsc(MissionName), *JsonEsc(Objective), bEngaged ? 1 : 0, ObjDist, Wave,
		Enemies, Credits, XP, Rank));

	// --- remember this sample for the next diff ---
	Prev.bValid = true;
	Prev.Hull = Hull;
	Prev.Shield = Shield;
	Prev.Enemies = Enemies;
	Prev.MissionIndex = MissionIndex;
	Prev.Phase = Phase;
	Prev.bDocked = bDocked;
	Prev.bEngaged = bEngaged;
	Prev.WarpCharge = WarpC;
}
