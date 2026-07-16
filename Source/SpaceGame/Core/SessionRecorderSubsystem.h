// SpaceGame — bridge simulator. Session telemetry recorder (dev/analysis "record mode").
//
// An opt-in mode that samples gameplay state to a newline-delimited JSON (JSONL) file under
// Saved/SessionLogs/ so a played session can be analysed offline — movement tracks, combat,
// hull loss, mission pacing, where players die — to tune the game against real play data.
//
// Off by default. Turn it on with the `sg.RecordSession 1` console variable (or `-recordsession`
// on the command line); flip it back to 0 to close the file. One file per recording; each line is
// a self-contained JSON object tagged by kind ("meta" | "s" sample | "e" event | "end"), so a
// crash still leaves a readable, parseable log up to the last flushed line. See tools/analyse_session.py.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SessionRecorderSubsystem.generated.h"

class ASpaceship;
class IFileHandle;

/**
 * USessionRecorderSubsystem — per-world telemetry recorder. Lives only in Game/PIE worlds; ticks
 * every frame but only writes a sample every SampleInterval seconds while recording is enabled and
 * a player ship exists. Discrete events (damage, kill, warp, dock, mission advance, outcome) are
 * derived by diffing successive samples, so nothing in the gameplay code has to know it's watched.
 */
UCLASS()
class SPACEGAME_API USessionRecorderSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// Only real play worlds (Game + PIE) — never editor/preview/inactive worlds.
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return !IsTemplate(); }

	/** True while a session file is open and being written. */
	bool IsRecording() const { return FileHandle.IsValid(); }

private:
	void StartRecording(const ASpaceship* Ship);
	void StopRecording(const TCHAR* Reason);
	void WriteLine(const FString& JsonObject);

	/** Diff live state against the previous sample, emit any "e" events, write one "s" sample. */
	void RecordSample(const ASpaceship* Ship);

	/** Count living enemy ships in this world (fleet size / kill detection). */
	int32 CountLiveEnemies() const;

	/** Encounter phase as a small int: 0 playing, 1 victory, 2 defeat. */
	int32 GetPhaseCode() const;

	ASpaceship* GetShip() const;

	TUniquePtr<IFileHandle> FileHandle;
	FString FilePath;

	/** World-time accumulated since the last written sample. */
	float SampleAccum = 0.f;

	/** Unix + world time at StartRecording, for the "end" duration and the meta header. */
	double StartUnixTime = 0.0;
	float StartWorldTime = 0.f;

	/** Previous sample, for delta-based event detection. */
	struct FSnapshot
	{
		bool bValid = false;
		float Hull = 0.f;
		float Shield = 0.f;
		int32 Enemies = 0;
		int32 MissionIndex = 0;
		int32 Phase = 0;
		bool bDocked = false;
		bool bEngaged = false;
		float WarpCharge = 0.f;
	};
	FSnapshot Prev;
};
