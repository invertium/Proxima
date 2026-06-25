// SpaceGame — bridge simulator. LAN web-station server implementation (M16).

#include "Net/StationServerSubsystem.h"

#include "Components/HealthComponent.h"
#include "Components/PowerComponent.h"
#include "Components/ShipMovementComponent.h"
#include "Components/WeaponComponent.h"
#include "Engine/World.h"
#include "HttpServerModule.h"
#include "HttpServerResponse.h"
#include "IPAddress.h"
#include "Kismet/GameplayStatics.h"
#include "Ships/Spaceship.h"
#include "SocketSubsystem.h"

namespace
{
	// --- Embedded station web client ---------------------------------------------------
	// One responsive page per station, served verbatim. JS polls /api/state every 250ms
	// and POSTs commands to /api/<station>. Kept dependency-free so any LAN browser works.

	const TCHAR* const IndexHtml = TEXT(R"HTML(<!doctype html><html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>SpaceGame — Bridge</title>
<style>
 body{margin:0;background:#05080f;color:#cfe6ff;font-family:system-ui,sans-serif;text-align:center}
 h1{padding:24px 0 4px;font-weight:600;letter-spacing:2px}
 p{color:#6f86a8;margin:0 0 28px}
 a.s{display:block;margin:14px auto;max-width:480px;padding:26px;border-radius:14px;
   text-decoration:none;color:#eaf4ff;font-size:1.5rem;letter-spacing:1px;
   background:#101a2c;border:1px solid #20406a}
 a.s:active{background:#16263f}
 .h{border-color:#2a6}.w{border-color:#a33}.e{border-color:#a82}
</style></head><body>
<h1>BRIDGE STATIONS</h1><p>pick a console</p>
<a class="s h" href="/helm">HELM</a>
<a class="s w" href="/weapons">WEAPONS</a>
<a class="s e" href="/engineering">ENGINEERING</a>
</body></html>)HTML");

	// Station page template. {TITLE} {ACCENT} {BODY} {SCRIPT} are substituted in.
	const TCHAR* const PageShell = TEXT(R"HTML(<!doctype html><html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>{TITLE}</title>
<style>
 body{margin:0;background:#05080f;color:#cfe6ff;font-family:system-ui,sans-serif;
   -webkit-user-select:none;user-select:none;overflow:hidden}
 header{padding:12px 16px;font-weight:600;letter-spacing:3px;border-bottom:1px solid {ACCENT};
   display:flex;justify-content:space-between;align-items:center}
 header a{color:#6f86a8;text-decoration:none;font-size:.8rem;letter-spacing:1px}
 .wrap{padding:18px;max-width:560px;margin:0 auto}
 .stat{display:flex;justify-content:space-between;padding:8px 4px;border-bottom:1px solid #15243a;font-size:1.05rem}
 .stat b{color:#8fb8e6;font-weight:600}
 button{font:inherit;color:#eaf4ff;background:#101a2c;border:1px solid {ACCENT};
   border-radius:12px;padding:20px;width:100%;font-size:1.25rem;letter-spacing:1px;margin-top:14px}
 button:active{background:#16263f}
 .row{display:flex;gap:12px}.row button{margin-top:12px}
 input[type=range]{width:100%;height:42px;margin-top:16px}
 label{display:block;margin-top:18px;color:#8fb8e6;letter-spacing:1px;font-size:.9rem}
 .big{font-size:1.6rem;color:#eaf4ff}
</style></head><body>
<header><span>{TITLE}</span><a href="/stations">&larr; stations</a></header>
<div class="wrap">{BODY}</div>
<script>
const $=s=>document.querySelector(s);
function post(path){fetch(path);}
async function poll(){try{const r=await fetch('/api/state');const s=await r.json();render(s);}catch(e){}}
{SCRIPT}
setInterval(poll,250);poll();
</script></body></html>)HTML");

	FString MakePage(const FString& Title, const FString& Accent, const FString& Body, const FString& Script)
	{
		FString Html = PageShell;
		Html = Html.Replace(TEXT("{TITLE}"), *Title);
		Html = Html.Replace(TEXT("{ACCENT}"), *Accent);
		Html = Html.Replace(TEXT("{BODY}"), *Body);
		Html = Html.Replace(TEXT("{SCRIPT}"), *Script);
		return Html;
	}

	FString HelmPage()
	{
		const FString Body = TEXT(
			"<div class='stat'><b>SPEED</b><span id='spd' class='big'>0</span></div>"
			"<div class='stat'><b>THROTTLE</b><span id='thr'>0%</span></div>"
			"<div class='stat'><b>MAX SPEED</b><span id='mx'>0</span></div>"
			"<label>THROTTLE</label>"
			"<input id='t' type='range' min='0' max='100' value='0' "
			"oninput=\"post('/api/helm?throttle='+(this.value/100))\">"
			"<label>TURN</label>"
			"<div class='row'>"
			"<button ontouchstart=\"post('/api/helm?turn=-1')\" onmousedown=\"post('/api/helm?turn=-1')\" "
			"ontouchend=\"post('/api/helm?turn=0')\" onmouseup=\"post('/api/helm?turn=0')\">&#9664; PORT</button>"
			"<button ontouchstart=\"post('/api/helm?turn=1')\" onmousedown=\"post('/api/helm?turn=1')\" "
			"ontouchend=\"post('/api/helm?turn=0')\" onmouseup=\"post('/api/helm?turn=0')\">STBD &#9654;</button>"
			"</div>");
		const FString Script = TEXT(
			"function render(s){$('#spd').textContent=Math.round(s.speed);"
			"$('#thr').textContent=Math.round(s.throttle*100)+'%';"
			"$('#mx').textContent=Math.round(s.maxSpeed);}");
		return MakePage(TEXT("HELM"), TEXT("#2a6"), Body, Script);
	}

	FString WeaponsPage()
	{
		const FString Body = TEXT(
			"<div class='stat'><b>CHARGE</b><span id='chg' class='big'>0%</span></div>"
			"<div class='stat'><b>TARGET</b><span id='tgt'>none</span></div>"
			"<div class='stat'><b>RANGE</b><span id='rng'>-</span></div>"
			"<div class='stat'><b>IN RANGE</b><span id='inr'>-</span></div>"
			"<button onclick=\"post('/api/weapons?action=cycle')\">CYCLE TARGET</button>"
			"<button onclick=\"post('/api/weapons?action=fire')\">FIRE BEAM</button>");
		const FString Script = TEXT(
			"function render(s){$('#chg').textContent=Math.round(s.charge*100)+'%';"
			"$('#tgt').textContent=s.target;"
			"$('#rng').textContent=s.targetRange<0?'-':Math.round(s.targetRange);"
			"$('#inr').textContent=s.inRange?'YES':'no';}");
		return MakePage(TEXT("WEAPONS"), TEXT("#a33"), Body, Script);
	}

	FString EngineeringPage()
	{
		// Three systems (Engine/Weapons/Shields) with -/+ nudge buttons and a power readout.
		auto Sys = [](const TCHAR* Name, int32 Idx) -> FString
		{
			return FString::Printf(TEXT(
				"<label>%s</label>"
				"<div class='stat'><b>POWER</b><span id='p%d' class='big'>100%%</span></div>"
				"<div class='row'>"
				"<button onclick=\"post('/api/power?system=%d&delta=-0.1')\">&#8211;</button>"
				"<button onclick=\"post('/api/power?system=%d&delta=0.1')\">+</button>"
				"</div>"), Name, Idx, Idx, Idx);
		};
		const FString Body = Sys(TEXT("ENGINE"), 0) + Sys(TEXT("WEAPONS"), 1) + Sys(TEXT("SHIELDS"), 2) +
			TEXT("<div class='stat' style='margin-top:18px'><b>REACTOR LOAD</b><span id='load'>-</span></div>"
			     "<div class='stat'><b>HULL</b><span id='hull'>-</span></div>");
		const FString Script = TEXT(
			"function render(s){$('#p0').textContent=Math.round(s.power[0]*100)+'%';"
			"$('#p1').textContent=Math.round(s.power[1]*100)+'%';"
			"$('#p2').textContent=Math.round(s.power[2]*100)+'%';"
			"$('#load').textContent=s.reactorLoad.toFixed(1)+' / '+s.reactorBudget.toFixed(1);"
			"$('#hull').textContent=Math.round(s.hull)+' / '+Math.round(s.maxHull);}");
		return MakePage(TEXT("ENGINEERING"), TEXT("#a82"), Body, Script);
	}

	// Build a response with the given body + content type.
	TUniquePtr<FHttpServerResponse> MakeResponse(const FString& Body, const FString& ContentType)
	{
		return FHttpServerResponse::Create(Body, ContentType);
	}

	// Read a numeric query param (defaulting if missing/unparseable).
	float QueryFloat(const FHttpServerRequest& Request, const FString& Key, float Default)
	{
		if (const FString* Val = Request.QueryParams.Find(Key))
		{
			return FCString::Atof(**Val);
		}
		return Default;
	}
}

bool UStationServerSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UStationServerSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	FHttpServerModule& Http = FHttpServerModule::Get();
	Router = Http.GetHttpRouter(Port);
	if (!Router.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[StationServer] could not get HTTP router on port %d"), Port);
		return;
	}

	// Bind GET pages + the state poll, and POST command endpoints. Keep handles to unbind.
	auto Bind = [this](const TCHAR* Path, EHttpServerRequestVerbs Verb,
		bool (UStationServerSubsystem::*Fn)(const FHttpServerRequest&, const FHttpResultCallback&))
	{
		FHttpRouteHandle Handle = Router->BindRoute(FHttpPath(Path), Verb,
			FHttpRequestHandler::CreateUObject(this, Fn));
		if (Handle.IsValid())
		{
			RouteHandles.Add(Handle);
		}
	};

	// NB: the HTTP server rejects the bare root path "/" (FHttpPath::IsValidPath), so the
	// landing page lives at /stations and that's the URL we advertise.
	Bind(TEXT("/stations"), EHttpServerRequestVerbs::VERB_GET, &UStationServerSubsystem::HandleIndex);
	Bind(TEXT("/helm"), EHttpServerRequestVerbs::VERB_GET, &UStationServerSubsystem::HandleStationPage);
	Bind(TEXT("/weapons"), EHttpServerRequestVerbs::VERB_GET, &UStationServerSubsystem::HandleStationPage);
	Bind(TEXT("/engineering"), EHttpServerRequestVerbs::VERB_GET, &UStationServerSubsystem::HandleStationPage);
	// Command endpoints are GET (with query params): UE's HTTP server rejects POSTs that
	// lack a Content-Length header, which empty browser fetch() POSTs don't reliably send.
	// These are simple idempotent-ish console commands on a LAN, so GET is fine.
	Bind(TEXT("/api/state"), EHttpServerRequestVerbs::VERB_GET, &UStationServerSubsystem::HandleState);
	Bind(TEXT("/api/helm"), EHttpServerRequestVerbs::VERB_GET, &UStationServerSubsystem::HandleHelm);
	Bind(TEXT("/api/weapons"), EHttpServerRequestVerbs::VERB_GET, &UStationServerSubsystem::HandleWeapons);
	Bind(TEXT("/api/power"), EHttpServerRequestVerbs::VERB_GET, &UStationServerSubsystem::HandlePower);

	Http.StartAllListeners();
	bListening = true;

	const FString Lan = GetLanAddress();
	const TCHAR* Host = Lan.IsEmpty() ? TEXT("<this-machine-LAN-IP>") : *Lan;
	UE_LOG(LogTemp, Log, TEXT("[StationServer] bridge stations live at  http://%s:%d/stations   (open on any LAN device)"),
		Host, Port);
}

void UStationServerSubsystem::Deinitialize()
{
	if (Router.IsValid())
	{
		for (const FHttpRouteHandle& Handle : RouteHandles)
		{
			if (Handle.IsValid())
			{
				Router->UnbindRoute(Handle);
			}
		}
	}
	RouteHandles.Reset();
	Router.Reset();

	if (bListening)
	{
		// Stops listeners for all routers, but ours are now unbound so this is safe in PIE.
		FHttpServerModule::Get().StopAllListeners();
		bListening = false;
	}

	Super::Deinitialize();
}

ASpaceship* UStationServerSubsystem::GetShip() const
{
	if (const UWorld* World = GetWorld())
	{
		return Cast<ASpaceship>(UGameplayStatics::GetPlayerPawn(World, 0));
	}
	return nullptr;
}

FString UStationServerSubsystem::GetLanAddress()
{
	ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!Sockets)
	{
		return FString();
	}

	TArray<TSharedPtr<FInternetAddr>> Addresses;
	if (Sockets->GetLocalAdapterAddresses(Addresses))
	{
		for (const TSharedPtr<FInternetAddr>& Addr : Addresses)
		{
			if (Addr.IsValid() && Addr->IsValid())
			{
				const FString Str = Addr->ToString(false); // host only, no port
				if (!Str.StartsWith(TEXT("127."))) // skip loopback
				{
					return Str;
				}
			}
		}
	}
	return FString();
}

bool UStationServerSubsystem::HandleIndex(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	OnComplete(MakeResponse(IndexHtml, TEXT("text/html; charset=utf-8")));
	return true;
}

bool UStationServerSubsystem::HandleStationPage(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	FString Html;
	if (Request.RelativePath.GetPath() == TEXT("/weapons"))
	{
		Html = WeaponsPage();
	}
	else if (Request.RelativePath.GetPath() == TEXT("/engineering"))
	{
		Html = EngineeringPage();
	}
	else
	{
		Html = HelmPage();
	}
	OnComplete(MakeResponse(Html, TEXT("text/html; charset=utf-8")));
	return true;
}

bool UStationServerSubsystem::HandleState(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	const ASpaceship* Ship = GetShip();
	const UShipMovementComponent* Move = Ship ? Ship->GetMovementComp() : nullptr;
	const UWeaponComponent* Weap = Ship ? Ship->GetWeaponComp() : nullptr;
	const UPowerComponent* Power = Ship ? Ship->GetPowerComp() : nullptr;
	const UHealthComponent* Health = Ship ? Ship->GetHealthComp() : nullptr;

	const AActor* Target = Weap ? Weap->GetCurrentTarget() : nullptr;
	const FString TargetName = Target ? Target->GetName() : TEXT("none");

	auto P = [Power](EShipSystem Sys) { return Power ? Power->GetSystemPower(Sys) : 0.f; };

	// Hand-built JSON (avoids pulling in the Json module for this flat object).
	const FString Json = FString::Printf(TEXT(
		"{\"speed\":%.1f,\"throttle\":%.3f,\"maxSpeed\":%.1f,"
		"\"charge\":%.3f,\"target\":\"%s\",\"targetRange\":%.1f,\"inRange\":%s,"
		"\"power\":[%.3f,%.3f,%.3f],\"reactorLoad\":%.3f,\"reactorBudget\":%.3f,"
		"\"hull\":%.1f,\"maxHull\":%.1f}"),
		Move ? Move->GetSpeed() : 0.f,
		Move ? Move->GetThrottle() : 0.f,
		Move ? Move->GetEffectiveMaxSpeed() : 0.f,
		Weap ? Weap->GetCharge() : 0.f,
		*TargetName,
		Weap ? Weap->GetTargetRange() : -1.f,
		(Weap && Weap->IsTargetInRange()) ? TEXT("true") : TEXT("false"),
		P(EShipSystem::Engine), P(EShipSystem::Weapons), P(EShipSystem::Shields),
		Power ? Power->GetTotalPower() : 0.f,
		Power ? Power->ReactorBudget : 0.f,
		Health ? Health->GetHull() : 0.f,
		Health ? Health->GetMaxHull() : 0.f);

	OnComplete(MakeResponse(Json, TEXT("application/json")));
	return true;
}

bool UStationServerSubsystem::HandleHelm(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	if (ASpaceship* Ship = GetShip())
	{
		if (UShipMovementComponent* Move = Ship->GetMovementComp())
		{
			if (Request.QueryParams.Contains(TEXT("throttle")))
			{
				Move->SetThrottle(QueryFloat(Request, TEXT("throttle"), 0.f));
			}
			if (Request.QueryParams.Contains(TEXT("turn")))
			{
				Move->SetTurn(QueryFloat(Request, TEXT("turn"), 0.f));
			}
		}
	}
	OnComplete(MakeResponse(TEXT("ok"), TEXT("text/plain")));
	return true;
}

bool UStationServerSubsystem::HandleWeapons(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	if (ASpaceship* Ship = GetShip())
	{
		if (UWeaponComponent* Weap = Ship->GetWeaponComp())
		{
			const FString* Action = Request.QueryParams.Find(TEXT("action"));
			if (Action && *Action == TEXT("cycle"))
			{
				Weap->CycleTarget();
			}
			else if (Action && *Action == TEXT("fire"))
			{
				Weap->FireBeam();
			}
		}
	}
	OnComplete(MakeResponse(TEXT("ok"), TEXT("text/plain")));
	return true;
}

bool UStationServerSubsystem::HandlePower(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	if (ASpaceship* Ship = GetShip())
	{
		if (UPowerComponent* Power = Ship->GetPowerComp())
		{
			const int32 Sys = FMath::Clamp((int32)QueryFloat(Request, TEXT("system"), -1.f), 0, 2);
			if (Request.QueryParams.Contains(TEXT("system")))
			{
				Power->AdjustSystemPower((EShipSystem)Sys, QueryFloat(Request, TEXT("delta"), 0.f));
			}
		}
	}
	OnComplete(MakeResponse(TEXT("ok"), TEXT("text/plain")));
	return true;
}
