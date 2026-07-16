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

	/** The URL crew devices open (e.g. "http://192.168.1.10:8080/stations"), or empty when no
	 *  LAN adapter was found. Shown on the main/pause menus so nobody has to read the log (R2). */
	static FString GetCrewUrl();

	/** Session access PIN (R5): 4 digits, generated once per game session. Shown in-game as
	 *  part of the crew URL; every HTTP route rejects requests that don't carry it. */
	static const FString& GetSessionPin();

private:
	// --- Route handlers (all run on the game thread) ---
	bool HandleIndex(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	// StationId payload (0=Helm,1=Weapons,2=Engineering) is bound per route, since exact-
	// matched routes don't carry the path in Request.RelativePath.
	bool HandleStationPage(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete, int32 StationId);
	bool HandleState(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleStarmap(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleHelm(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleDock(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleWarp(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleBuy(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleShip(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleWeapons(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandlePower(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	// Damage-control repair (timing-sweep minigame); rate-limited so it can't be spammed to full.
	bool HandleEngineering(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	// Science: cycle scan target + run a scan that reveals the contact's hull/shield.
	bool HandleScience(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	// New Game / Restart — reloads the encounter.
	bool HandleGame(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleContract(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleAlert(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

	/** Resolve the local player ship in this world, or null if not spawned yet. */
	ASpaceship* GetShip() const;

	/** Gate for the ship-command endpoints: the ship must exist, be alive, and the encounter
	 *  still be Playing. Returns null and fills OutReason when the command should be rejected
	 *  (R5 API honesty — commands against a dead ship / ended encounter report why). */
	ASpaceship* GetCommandShip(FString& OutReason) const;

	/** Best-guess LAN IPv4 of this machine (skips loopback), or empty. */
	static FString GetLanAddress();

	/** Router for our port; kept so we can unbind our routes on teardown. */
	TSharedPtr<IHttpRouter> Router;

	/** Routes we bound, unbound on Deinitialize so a later PIE run rebinds cleanly. */
	TArray<FHttpRouteHandle> RouteHandles;

	/** Request preprocessor that bounces "/" to /stations (the router can't route the bare root). */
	FDelegateHandle RootRedirectHandle;

	/** Port the router is bound to this session (chosen at begin play — 8080, or the next free port
	 *  if something else already holds it). */
	int32 Port = 8080;

	/** Static mirror of the bound port so the static GetCrewUrl() can advertise the right one. */
	static int32 BoundPort;

	/** World-time of the last credited repair, to throttle the Engineering weld minigame. */
	double LastRepairTime = 0.0;
};
