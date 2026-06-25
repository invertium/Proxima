// SpaceGame — bridge simulator. LAN web-station server (M16).
//
// Hosts an in-process HTTP server (port 8080) so other devices on the LAN — phones,
// tablets, laptops — open a browser to http://<this-machine-LAN-IP>:8080 and operate
// the Helm / Weapons / Engineering stations. The main machine stays the "viewscreen"
// (server + ship view); the web clients are the bridge consoles. Single-machine play
// still works via the local 1/2/3 console overlays.
//
// HttpServer dispatches handlers on the game thread during module Tick, so the handlers
// touch the ship's UObjects directly (no marshalling needed).

#pragma once

#include "CoreMinimal.h"
#include "HttpResultCallback.h"
#include "IHttpRouter.h"
#include "Subsystems/WorldSubsystem.h"
#include "StationServerSubsystem.generated.h"

class ASpaceship;
struct FHttpServerRequest;

/**
 * UStationServerSubsystem — spins up a tiny HTTP server for the play world so LAN
 * devices can drive the bridge stations. Lives only in Game/PIE worlds.
 */
UCLASS()
class SPACEGAME_API UStationServerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// Only real play worlds (Game + PIE) — never editor/preview/inactive worlds.
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

private:
	// --- Route handlers (all run on the game thread) ---
	bool HandleIndex(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	// StationId payload (0=Helm,1=Weapons,2=Engineering) is bound per route, since exact-
	// matched routes don't carry the path in Request.RelativePath.
	bool HandleStationPage(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete, int32 StationId);
	bool HandleState(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleHelm(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleWeapons(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandlePower(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

	/** Resolve the local player ship in this world, or null if not spawned yet. */
	ASpaceship* GetShip() const;

	/** Best-guess LAN IPv4 of this machine (skips loopback), or empty. */
	static FString GetLanAddress();

	/** Router for our port; kept so we can unbind our routes on teardown. */
	TSharedPtr<IHttpRouter> Router;

	/** Routes we bound, unbound on Deinitialize so a later PIE run rebinds cleanly. */
	TArray<FHttpRouteHandle> RouteHandles;

	/** Port the router is bound to. */
	int32 Port = 8080;
};
