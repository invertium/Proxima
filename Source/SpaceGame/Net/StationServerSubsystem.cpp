// SpaceGame — bridge simulator. LAN web-station server implementation (M16).

#include "Net/StationServerSubsystem.h"

#include "Components/HealthComponent.h"
#include "Components/PowerComponent.h"
#include "Components/RadarContactComponent.h"
#include "Components/ScienceComponent.h"
#include "Components/ShipMovementComponent.h"
#include "Components/TorpedoLauncherComponent.h"
#include "Components/WeaponComponent.h"
#include "Core/MissionSubsystem.h"
#include "Core/ShipCatalogue.h"
#include "Core/SpaceGameInstance.h"
#include "Core/SpaceGameMode.h"
#include "Core/UpgradeCatalogue.h"
#include "Core/StationTypes.h"
#include "Engine/World.h"
#include "HttpServerModule.h"
#include "HttpServerResponse.h"
#include "IPAddress.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Ships/EnemyShip.h"
#include "Ships/Spaceship.h"
#include "World/Station.h"
#include "SocketSubsystem.h"
#include "UObject/UObjectIterator.h"

// World distance (uu) from the Helm map centre to the outer ring — mirrors URadarWidget::RadarRangeUU.
static constexpr float HelmRadarRangeUU = 20000.f;

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
 p{color:#6f86a8;margin:0 0 20px}
 #state{max-width:480px;margin:0 auto 22px;padding:14px;border-radius:12px;font-weight:600;
   letter-spacing:2px;border:1px solid #15243a;background:#0a1018}
 #state.live{color:#3fbf6f}#state.win{color:#43ff7a;background:#0c2417}#state.lose{color:#ff5a4a;background:#2a0c0c}
 a.s{display:block;margin:14px auto;max-width:480px;padding:26px;border-radius:14px;
   text-decoration:none;color:#eaf4ff;font-size:1.5rem;letter-spacing:1px;
   background:#101a2c;border:1px solid #20406a}
 a.s:active{background:#16263f}
 .h{border-color:#2a6}.w{border-color:#a33}.e{border-color:#a82}.sc{border-color:#37c}
 .ctl{display:flex;gap:12px;max-width:480px;margin:26px auto 0}
 .ctl button{flex:1;font:inherit;color:#eaf4ff;background:#101a2c;border:1px solid #2b4c78;
   border-radius:12px;padding:18px;font-size:1.05rem;letter-spacing:1px}
 .ctl button:active{background:#16263f}
</style></head><body>
<h1>BRIDGE STATIONS</h1><p>pick a console</p>
<div id="state" class="live">●</div>
<a class="s h" href="/helm">HELM</a>
<a class="s w" href="/weapons">WEAPONS</a>
<a class="s e" href="/engineering">ENGINEERING</a>
<a class="s sc" href="/science">SCIENCE</a>
<div class="ctl">
<button onclick="if(confirm('Start a new game for everyone?'))game('new')">NEW GAME</button>
<button onclick="if(confirm('Restart the encounter for everyone?'))game('restart')">RESTART</button>
</div>
<script>
function game(a){fetch('/api/game?action='+a);}
async function poll(){try{const s=await(await fetch('/api/state')).json();const e=document.getElementById('state');
 if(s.phase==='victory'){e.textContent='✔ VICTORY — all hostiles destroyed';e.className='win';}
 else if(s.phase==='defeat'){e.textContent='✖ DEFEAT — ship destroyed';e.className='lose';}
 else if(s.staged){e.textContent='◇ STANDING BY — resupply, then LAUNCH at the Helm';e.className='live';}
 else{e.textContent='● ENCOUNTER LIVE';e.className='live';}}catch(e){}}
setInterval(poll,500);poll();
</script>
</body></html>)HTML");

	// Station page template. {TITLE} {ACCENT} {BODY} {SCRIPT} are substituted in.
	const TCHAR* const PageShell = TEXT(R"HTML(<!doctype html><html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>{TITLE}</title>
<style>
 body{margin:0;background:#05080f;color:#cfe6ff;font-family:system-ui,sans-serif;
   -webkit-user-select:none;user-select:none;
   overflow-x:hidden;overflow-y:auto;-webkit-overflow-scrolling:touch;overscroll-behavior-y:contain}
 header{padding:12px 16px;font-weight:600;letter-spacing:3px;border-bottom:1px solid {ACCENT};
   display:flex;justify-content:space-between;align-items:center}
 header a{color:#6f86a8;text-decoration:none;font-size:.8rem;letter-spacing:1px}
 .wrap{padding:18px;max-width:560px;margin:0 auto}
 .stat{display:flex;justify-content:space-between;padding:8px 4px;border-bottom:1px solid #15243a;font-size:1.05rem}
 .stat b{color:#8fb8e6;font-weight:600}
 button{font:inherit;color:#eaf4ff;background:#101a2c;border:1px solid {ACCENT};
   border-radius:12px;padding:20px;width:100%;font-size:1.25rem;letter-spacing:1px;margin-top:14px}
 button:active{background:#16263f}
 button.rdy{border-color:#43ff7a;color:#aef0c0;background:#0e2418}
 button.blk{color:#6f86a8;border-color:#33425c;background:#0b1220}
 .row{display:flex;gap:12px}.row button{margin-top:12px}
 input[type=range]{width:100%;height:42px;margin-top:16px}
 label{display:block;margin-top:18px;color:#8fb8e6;letter-spacing:1px;font-size:.9rem}
 .big{font-size:1.6rem;color:#eaf4ff}
 .wrap{padding-bottom:74px}
 footer.gs{position:fixed;left:0;right:0;bottom:0;display:flex;justify-content:space-between;
   align-items:center;padding:10px 16px;letter-spacing:2px;border-top:1px solid #15243a;background:#070b14}
 footer.gs #gsl{font-weight:600;font-size:1rem}
 footer.gs.live #gsl{color:#3fbf6f}
 footer.gs.win{background:#0c2417}footer.gs.win #gsl{color:#43ff7a}
 footer.gs.lose{background:#2a0c0c}footer.gs.lose #gsl{color:#ff5a4a}
 footer.gs button{width:auto;margin:0;padding:9px 16px;font-size:.85rem;border-radius:8px}
</style></head><body>
<header><span>{TITLE}</span><a href="/stations">&larr; stations</a></header>
<div class="wrap">{BODY}</div>
<div id="smap" style="display:none;position:fixed;inset:0;z-index:50;background:rgba(4,7,14,.95);
 flex-direction:column;align-items:center;justify-content:center;padding:16px">
<div id="smaptitle" style="letter-spacing:3px;color:#8fb8e6;margin-bottom:6px;font-weight:600">SECTOR MAP</div>
<canvas id="smapc" width="560" height="380" style="max-width:94vw;max-height:62vh;background:#060b14;border:1px solid #20406a;border-radius:12px"></canvas>
<button onclick="closeStarmap()" style="width:auto;margin-top:16px;padding:12px 30px">&times; CLOSE</button></div>
<footer id="gs" class="gs live"><span id="gsl">●</span>
<button onclick="if(confirm('Restart the encounter for everyone?'))post('/api/game?action=restart')">&#8635; RESTART</button></footer>
<script>
const $=s=>document.querySelector(s);
function post(path){fetch(path);}
function renderFooter(s){const f=$('#gs'),l=$('#gsl');
 if(s.phase==='victory'){l.textContent='✔ VICTORY';f.className='gs win';}
 else if(s.phase==='defeat'){l.textContent='✖ DEFEAT';f.className='gs lose';}
 else if(s.staged){l.textContent='◇ STANDING BY';f.className='gs live';}
 else{l.textContent='● ENCOUNTER LIVE';f.className='gs live';}}
async function poll(){try{const r=await fetch('/api/state');const s=await r.json();render(s);renderFooter(s);}catch(e){}}
async function openStarmap(){try{const d=await(await fetch('/api/starmap')).json();drawStarmap(d);$('#smap').style.display='flex';}catch(e){}}
function closeStarmap(){$('#smap').style.display='none';}
function drawStarmap(d){const c=$('#smapc'),x=c.getContext('2d'),W=c.width,H=c.height,pad=48;
 $('#smaptitle').textContent=(d.sector||'SECTOR MAP');
 const X=v=>pad+v*(W-2*pad),Y=v=>pad+v*(H-2*pad),sys=d.systems||[];
 x.clearRect(0,0,W,H);
 x.fillStyle='#0a1322';for(let i=0;i<70;i++){x.fillRect((i*97)%W,(i*53)%H,1,1);}
 x.strokeStyle='rgba(90,140,210,.45)';x.lineWidth=2;x.setLineDash([7,7]);x.beginPath();
 for(let i=0;i<sys.length;i++){const px=X(sys[i].x),py=Y(sys[i].y);i?x.lineTo(px,py):x.moveTo(px,py);}
 x.stroke();x.setLineDash([]);
 for(const s of sys){const px=X(s.x),py=Y(s.y);
  const col=s.status==='cleared'?'#43ff7a':(s.status==='current'?'#ffd24a':'#5b6b82');
  if(s.status==='current'){x.strokeStyle='rgba(255,210,74,.55)';x.lineWidth=2;x.beginPath();x.arc(px,py,15,0,7);x.stroke();}
  x.fillStyle=col;x.beginPath();x.arc(px,py,7,0,7);x.fill();
  x.fillStyle=s.status==='locked'?'#7184a0':'#e6eefb';x.font='600 14px system-ui';x.textAlign='center';
  x.fillText(s.name,px,py-22);
  x.fillStyle='#6f86a8';x.font='10px system-ui';x.fillText(s.status.toUpperCase(),px,py+28);}}
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
		// Top-down tactical map (north-up, player-centred) mirroring URadarWidget, drawn on a
		// canvas, plus a numeric heading readout and the throttle/turn controls.
		const FString Body = TEXT(
			// Native 320x320 buffer (drawMap maths off c.width/height); CSS scales the *display* size so
			// the radar shrinks on short screens, keeping the flight controls within easy reach.
			"<canvas id='map' width='320' height='320' "
			"style='display:block;margin:6px auto 0;background:#070d16;border-radius:50%;border:1px solid #15243a;"
			"width:min(320px,86vw,40vh);height:min(320px,86vw,40vh)'></canvas>"
			"<div style='text-align:center;font-size:.8rem;letter-spacing:1px;margin-top:6px;color:#8fb8e6'>"
			"FIRING ARCS &nbsp; <span style='color:#ff7a5a'>&#9632; PHASER</span> &nbsp; "
			"<span style='color:#78b4ff'>&#9632; TORPEDO</span></div>"
			"<button onclick='openStarmap()' style='width:auto;margin:10px auto 0;display:block;"
			"padding:8px 18px;font-size:.9rem'>&#9678; SECTOR MAP</button>"
			"<div class='stat'><b>HEADING</b><span id='hdg' class='big'>0&deg;</span></div>"
			"<div class='stat'><b>SPEED</b><span id='spd'>0</span></div>"
			"<div class='stat'><b>THROTTLE</b><span id='thr'>0%</span></div>"
			"<div class='stat'><b>MAX SPEED</b><span id='mx'>0</span></div>"
			"<div class='stat'><b>STARBASE</b><span id='dock'>-</span></div>"
			"<button id='dockbtn' onclick=\"toggleDock()\">DOCK</button>"
			"<button id='launchbtn' onclick=\"post('/api/game?action=launch')\" "
			"style='display:none;background:#1c2e16;border-color:#43ff7a;color:#cd"
			"ffd9'>&#9654; LAUNCH MISSION</button>"
			"<div class='stat'><b>WARP DRIVE</b><span id='warp'>-</span></div>"
			"<button id='warpbtn' onclick=\"post('/api/warp')\">WARP</button>"
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
		// world +X = up (-screenY), world +Y = right (+screenX) — same mapping as RadarWidget.
		const FString Script = TEXT(
			"function render(s){"
			"$('#hdg').textContent=Math.round(((s.heading%360)+360)%360)+'\\u00b0';"
			"$('#spd').textContent=Math.round(s.speed);"
			"$('#thr').textContent=Math.round(s.throttle*100)+'%';"
			"$('#mx').textContent=Math.round(s.maxSpeed);"
			"window.dk=s.docked;"
			"$('#dock').textContent=s.docked?'DOCKED \\u2014 repaired & restocked'"
			":(s.canDock?'in range \\u2014 ready to dock'"
			":(s.stationRange<0?'no base':'range '+Math.round(s.stationRange)));"
			"const db=$('#dockbtn');db.textContent=s.docked?'\\u2191 UNDOCK':'\\u2193 DOCK';"
			"db.disabled=!s.docked&&!s.canDock;db.className=(s.docked||s.canDock)?'rdy':'blk';"
			"$('#launchbtn').style.display=s.staged?'block':'none';"
			"const wc=Math.round((s.warpCharge||0)*100),wb=$('#warpbtn');"
			"$('#warp').textContent=s.docked?'offline (docked)':(s.warpReady?'READY':wc+'%');"
			"wb.textContent=s.warpReady?'\\u27a4 WARP JUMP':'CHARGING '+wc+'%';"
			"wb.disabled=!s.warpReady;wb.className=s.warpReady?'rdy':'blk';"
			"drawMap(s);}"
			"function toggleDock(){post('/api/dock?action='+(window.dk?'undock':'dock'));}"
			"function drawMap(s){const c=$('#map'),x=c.getContext('2d');const W=c.width,H=c.height;"
			"const cx=W/2,cy=H/2,R=W/2-8;const scale=R/(s.radarRange||20000);"
			"x.clearRect(0,0,W,H);"
			"x.strokeStyle='rgba(40,115,90,.6)';x.lineWidth=1;"
			"for(const f of [1,.66,.33]){x.beginPath();x.arc(cx,cy,R*f,0,7);x.stroke();}"
			"x.strokeStyle='rgba(30,80,70,.55)';x.beginPath();"
			"x.moveTo(cx-R,cy);x.lineTo(cx+R,cy);x.moveTo(cx,cy-R);x.lineTo(cx,cy+R);x.stroke();"
			// Forward firing-arc wedges (helm aims by turning a target blip into a wedge). A wedge is
			// centred on the heading; world heading p maps to screen (sin p, -cos p), matching the bow
			// arrow below. Brighter fill when the locked target sits inside that weapon's arc.
			"const hr=s.heading*Math.PI/180;"
			"function wedge(halfDeg,fill,edge){const h=halfDeg*Math.PI/180;"
			"x.beginPath();x.moveTo(cx,cy);const N=20;"
			"for(let i=0;i<=N;i++){const p=hr-h+2*h*i/N;x.lineTo(cx+Math.sin(p)*R,cy-Math.cos(p)*R);}"
			"x.closePath();x.fillStyle=fill;x.fill();"
			"x.strokeStyle=edge;x.lineWidth=1.5;x.beginPath();"
			"x.moveTo(cx+Math.sin(hr-h)*R,cy-Math.cos(hr-h)*R);x.lineTo(cx,cy);"
			"x.lineTo(cx+Math.sin(hr+h)*R,cy-Math.cos(hr+h)*R);x.stroke();}"
			"const dP=()=>{if(s.phaserArc>0)wedge(s.phaserArc/2,"
			"s.inArc?'rgba(255,90,70,.32)':'rgba(255,90,70,.13)','rgba(255,120,90,.6)');};"
			"const dT=()=>{if(s.torpedoArc>0)wedge(s.torpedoArc/2,"
			"s.inArcTorp?'rgba(90,160,255,.30)':'rgba(90,160,255,.12)','rgba(130,185,255,.55)');};"
			"if(s.phaserArc>=s.torpedoArc){dP();dT();}else{dT();dP();}"
			"const w2s=(dx,dy)=>{let sx=dy*scale,sy=-dx*scale;const m=Math.hypot(sx,sy);"
			"if(m>R){sx=sx/m*R;sy=sy/m*R;}return [cx+sx,cy+sy];};"
			"for(const k of (s.contacts||[])){const[px,py]=w2s(k.x-s.px,k.y-s.py);"
			"x.strokeStyle=k.color||'#ff4030';x.lineWidth=1.5;"
			"x.beginPath();x.arc(px,py,5,0,7);x.stroke();"
			"x.beginPath();x.moveTo(px-7,py);x.lineTo(px+7,py);x.moveTo(px,py-7);x.lineTo(px,py+7);x.stroke();"
			"if(k.label){x.fillStyle=k.color||'#ff4030';x.font='11px system-ui';x.textAlign='left';"
			"x.fillText(k.label,Math.min(px+9,W-46),Math.max(11,py+3));}}"
			"const a=s.heading*Math.PI/180;const fx=Math.cos(a),fy=Math.sin(a);"
			"const sfx=fy,sfy=-fx;const tx=cx+sfx*22,ty=cy+sfy*22;const perpx=-sfy,perpy=sfx;"
			"x.strokeStyle='#33e6ff';x.lineWidth=2;"
			"x.beginPath();x.arc(cx,cy,4,0,7);x.stroke();"
			"x.beginPath();x.moveTo(cx,cy);x.lineTo(tx,ty);"
			"x.moveTo(tx,ty);x.lineTo(tx-sfx*8+perpx*5,ty-sfy*8+perpy*5);"
			"x.moveTo(tx,ty);x.lineTo(tx-sfx*8-perpx*5,ty-sfy*8-perpy*5);x.stroke();}");
		return MakePage(TEXT("HELM"), TEXT("#2a6"), Body, Script);
	}

	FString WeaponsPage()
	{
		const FString Body = TEXT(
			"<div class='stat'><b>CHARGE</b><span id='chg' class='big'>0%</span></div>"
			"<div class='stat'><b>TARGET</b><span id='tgt'>none</span></div>"
			"<div class='stat'><b>RANGE</b><span id='rng'>-</span></div>"
			"<div class='stat'><b>IN RANGE</b><span id='inr'>-</span></div>"
			"<div class='stat'><b>ON TARGET</b><span id='ina'>-</span></div>"
			"<div class='stat'><b>TORPEDOES</b><span id='ammo' class='big'>0</span></div>"
			"<button onclick=\"post('/api/weapons?action=cycle')\">CYCLE TARGET</button>"
			"<button id='fire' onclick=\"post('/api/weapons?action=fire')\">FIRE BEAM</button>"
			"<button id='torp' onclick=\"post('/api/weapons?action=torpedo')\">FIRE TORPEDO</button>");
		// Both FIRE buttons reflect readiness: green only when the shot will actually land,
		// otherwise dimmed with the reason. The beam needs charge + in-range; the torpedo
		// (which homes to the target and bypasses shields) needs only ammo + reload + a lock.
		const FString Script = TEXT(
			"function render(s){$('#chg').textContent=Math.round(s.charge*100)+'%';"
			"$('#tgt').textContent=s.target;"
			"$('#rng').textContent=s.targetRange<0?'-':Math.round(s.targetRange);"
			"$('#inr').textContent=s.inRange?'YES':'no';"
			"$('#ina').textContent=s.inArc?'YES':'no';$('#ina').style.color=s.target!=='none'&&!s.inArc?'#ffb300':'';"
			"$('#ammo').textContent=s.ammo+' / '+s.maxAmmo;"
			"const fb=$('#fire');"
			"if(s.target==='none'){fb.textContent='NO TARGET';fb.className='blk';}"
			"else if(s.charge<1){fb.textContent='CHARGING '+Math.round(s.charge*100)+'%';fb.className='blk';}"
			"else if(!s.inRange){fb.textContent='OUT OF RANGE';fb.className='blk';}"
			"else if(!s.inArc){fb.textContent='TURN TO TARGET';fb.className='blk';}"
			"else{fb.textContent='● FIRE BEAM';fb.className='rdy';}"
			"const tb=$('#torp');"
			"if(s.ammo<=0){tb.textContent='NO TORPEDOES';tb.className='blk';}"
			"else if(s.target==='none'){tb.textContent='NO TARGET';tb.className='blk';}"
			"else if(s.torpedoReload<1){tb.textContent='RELOADING '+Math.round(s.torpedoReload*100)+'%';tb.className='blk';}"
			"else if(!s.inArcTorp){tb.textContent='TURN TO TARGET';tb.className='blk';}"
			"else{tb.textContent='◎ FIRE TORPEDO';tb.className='rdy';}}");
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
		const FString Body =
			TEXT("<label>HULL INTEGRITY</label>"
			     "<div style='height:22px;background:#0b1220;border:1px solid #15243a;border-radius:6px;overflow:hidden'>"
			     "<div id='hullbar' style='height:100%;width:0%;background:#43ff7a;transition:width .2s'></div></div>"
			     "<div class='stat'><b>HULL</b><span id='hull' class='big'>-</span></div>")
			+ Sys(TEXT("ENGINE"), 0) + Sys(TEXT("WEAPONS"), 1) + Sys(TEXT("SHIELDS"), 2)
			+ TEXT(
			     "<label>DAMAGE CONTROL &mdash; WELD WHEN THE MARKER HITS THE GREEN</label>"
			     "<div id='sweep' style='position:relative;height:46px;margin-top:10px;background:#0b1220;"
			     "border:1px solid #33425c;border-radius:8px;overflow:hidden'>"
			     "<div style='position:absolute;left:40%;width:20%;top:0;bottom:0;background:rgba(67,255,122,.18);"
			     "border-left:1px solid #43ff7a;border-right:1px solid #43ff7a'></div>"
			     "<div id='mark' style='position:absolute;left:0;top:0;bottom:0;width:4px;background:#eaf4ff;"
			     "box-shadow:0 0 8px #fff'></div></div>"
			     "<button id='weld' onclick='weld()'>&#9670; WELD</button>"
			     "<div class='stat' style='margin-top:18px'><b>REACTOR LOAD</b><span id='load'>-</span></div>"
			     "<div id='drydock' style='margin-top:24px'>"
			     "<label>DRYDOCK &mdash; UPGRADES</label>"
			     "<div class='stat'><b>SALVAGE</b><span id='wallet' class='big'>-</span></div>"
			     "<div id='shop'></div>"
			     "<label style='margin-top:18px'>HANGAR &mdash; SHIPS</label>"
			     "<div id='hangar'></div></div>");
		// The hull bar + numeric readout poll from /api/state; the weld minigame runs entirely
		// client-side (requestAnimationFrame sweep) and only POSTs a repair when the marker is
		// inside the green zone — the server still rate-limits + caps the actual hull restore.
		const FString Script = TEXT(
			"function render(s){$('#p0').textContent=Math.round(s.power[0]*100)+'%';"
			"$('#p1').textContent=Math.round(s.power[1]*100)+'%';"
			"$('#p2').textContent=Math.round(s.power[2]*100)+'%';"
			"const ld=$('#load');ld.textContent=s.reactorLoad.toFixed(1)+' / '+s.reactorBudget.toFixed(1);"
			"const cap=s.reactorLoad>=s.reactorBudget-0.001;"
			"ld.style.color=cap?'#ffb300':'';ld.title=cap?'Reactor at capacity \\u2014 boost one system and another drops':'';"
			"const f=s.maxHull>0?s.hull/s.maxHull:0;"
			"$('#hullbar').style.width=Math.round(f*100)+'%';"
			"$('#hullbar').style.background=f<0.3?'#ff5a4a':(f<0.6?'#e6b800':'#43ff7a');"
			"$('#hull').textContent=Math.round(s.hull)+' / '+Math.round(s.maxHull);"
			// Drydock store: wallet + a buy row per upgrade. Active only while docked; otherwise dimmed.
			"$('#wallet').textContent=s.credits+' cr \\u00b7 RANK '+s.rank;"
			"const dd=$('#drydock'),shop=$('#shop');dd.style.opacity=s.docked?1:.5;"
			"if(!s.docked){shop.innerHTML='<div style=\"color:#6f86a8;padding:8px 2px\">Dock at the STARBASE (Helm) to outfit the ship.</div>';$('#hangar').innerHTML='';}"
			"else{let h='';for(const u of (s.upgrades||[])){"
			"let lbl=u.name+' '+(u.tier>0?('T'+u.tier):'\\u2014')+'/'+u.maxTier;"
			"let b;if(u.maxed){b='<span style=\"color:#43ff7a;font-size:.8rem\">MAX</span>';}"
			"else{let lock=u.rankReq>s.rank?(' \\u00b7 R'+u.rankReq):'';"
			"b='<button onclick=\"buy(\\''+u.id+'\\')\" '+(u.affordable?'':'disabled ')+"
			"'style=\"width:auto;margin:0;padding:8px 12px;font-size:.82rem;'+(u.affordable?'':'opacity:.4;')+'\">+'"
			"+u.mag+u.unit+' \\u00b7 '+u.cost+'cr'+lock+'</button>';}"
			"h+='<div style=\"display:flex;justify-content:space-between;align-items:center;padding:7px 2px;border-bottom:1px solid #15243a\"><span>'+lbl+'</span>'+b+'</div>';}"
			"shop.innerHTML=h;"
			// Hangar: a row per ship — ACTIVE / SELECT (owned) or BUY price (unowned, rank-gated).
			"let hh='';for(const v of (s.ships||[])){"
			"let b;if(v.active){b='<span style=\"color:#43ff7a;font-size:.8rem\">ACTIVE</span>';}"
			"else if(v.owned){b='<button onclick=\"selship('+v.id+')\" style=\"width:auto;margin:0;padding:8px 12px;font-size:.82rem\">SELECT</button>';}"
			"else{let lock=v.rankReq>s.rank?(' \\u00b7 R'+v.rankReq):'';"
			"b='<button onclick=\"buyship('+v.id+')\" '+(v.affordable?'':'disabled ')+"
			"'style=\"width:auto;margin:0;padding:8px 12px;font-size:.82rem;'+(v.affordable?'':'opacity:.4;')+'\">BUY \\u00b7 '+v.cost+'cr'+lock+'</button>';}"
			"hh+='<div style=\"display:flex;justify-content:space-between;align-items:center;padding:7px 2px;border-bottom:1px solid #15243a\"><span title=\"'+v.blurb+'\">'+v.name+'</span>'+b+'</div>';}"
			"$('#hangar').innerHTML=hh;}}"
			"function buy(id){post('/api/buy?id='+id);}"
			"function buyship(id){post('/api/ship?action=buy&id='+id);}"
			"function selship(id){post('/api/ship?action=select&id='+id);}"
			"let sp=0,sd=1,lt=0;"
			"function loop(ts){if(lt){const dt=(ts-lt)/1000;sp+=sd*dt/1.2;"
			"if(sp>=1){sp=1;sd=-1;}if(sp<=0){sp=0;sd=1;}}lt=ts;"
			"const m=$('#mark');if(m)m.style.left='calc('+(sp*100)+'% - 2px)';"
			"requestAnimationFrame(loop);}requestAnimationFrame(loop);"
			"function flash(c){const s=$('#sweep');s.style.boxShadow='0 0 14px '+c;setTimeout(function(){s.style.boxShadow='';},160);}"
			"function weld(){if(sp>=0.40&&sp<=0.60){post('/api/engineering?action=repair');flash('#43ff7a');}else{flash('#ff5a4a');}}");
		return MakePage(TEXT("ENGINEERING"), TEXT("#a82"), Body, Script);
	}

	FString SciencePage()
	{
		// Cycle to a contact, run a timed scan, then read its hull/shield live off the target.
		const FString Body = TEXT(
			"<div class='stat'><b>MISSION</b><span id='msn'>&mdash;</span></div>"
			"<button onclick='openStarmap()' style='width:auto;margin:10px auto 0;display:block;"
			"padding:8px 18px;font-size:.9rem'>&#9678; SECTOR MAP</button>"
			"<label>COMMS</label>"
			"<div id='comms' style='height:150px;overflow-y:auto;background:#0b1220;border:1px solid #15243a;"
			"border-radius:8px;padding:8px;text-align:left;font-size:.92rem;line-height:1.3'>"
			"<span style='color:#5a6f8a'>no transmissions</span></div>"
			"<div class='stat' style='margin-top:18px'><b>CONTACT</b><span id='tgt' class='big'>none</span></div>"
			"<label>SCAN</label>"
			"<div style='height:18px;background:#0b1220;border:1px solid #15243a;border-radius:6px;overflow:hidden'>"
			"<div id='scan' style='height:100%;width:0%;background:#37c;transition:width .15s'></div></div>"
			"<div class='stat'><b>STATUS</b><span id='st'>—</span></div>"
			"<button onclick=\"post('/api/science?action=cycle')\">CYCLE TARGET</button>"
			"<button id='scanbtn' onclick=\"post('/api/science?action=scan')\">SCAN</button>"
			"<div id='readout' style='margin-top:8px;opacity:.4'>"
			"<label>HULL</label>"
			"<div style='height:20px;background:#0b1220;border:1px solid #15243a;border-radius:6px;overflow:hidden'>"
			"<div id='hbar' style='height:100%;width:0%;background:#43ff7a;transition:width .2s'></div></div>"
			"<div class='stat'><b>HULL</b><span id='h'>—</span></div>"
			"<label>SHIELD</label>"
			"<div style='height:20px;background:#0b1220;border:1px solid #15243a;border-radius:6px;overflow:hidden'>"
			"<div id='sbar' style='height:100%;width:0%;background:#37c;transition:width .2s'></div></div>"
			"<div class='stat'><b>SHIELD</b><span id='sh'>—</span></div>"
			"</div>");
		const FString Script = TEXT(
			"function render(s){"
			"$('#msn').textContent=s.mission||'\\u2014';"
			"const cm=$('#comms'),items=(s.comms||[]);"
			"if(items.length){cm.innerHTML=items.map(function(c,i){return '<div style=\"padding:3px 0;'+"
			"(i===items.length-1?'color:#aef0c0':'color:#8fb8e6')+'\"><b>'+c.sender+':</b> '+c.text+'</div>';}).join('');"
			"if(String(items.length)!==cm.dataset.n){cm.dataset.n=items.length;cm.scrollTop=cm.scrollHeight;}}"
			"$('#tgt').textContent=s.sciTarget;"
			"$('#scan').style.width=Math.round(s.sciProgress*100)+'%';"
			"const sb=$('#scanbtn');"
			"if(s.sciTarget==='none'){$('#st').textContent='no contact';sb.textContent='SCAN';sb.className='blk';}"
			"else if(s.sciScanned){$('#st').textContent='SCAN COMPLETE';sb.textContent='SCANNED';sb.className='blk';}"
			"else if(s.sciScanning){$('#st').textContent='scanning '+Math.round(s.sciProgress*100)+'%';sb.textContent='SCANNING…';sb.className='blk';}"
			"else{$('#st').textContent='ready to scan';sb.textContent='◎ SCAN';sb.className='rdy';}"
			"const r=$('#readout');"
			"if(s.sciScanned&&s.sciHull>=0){r.style.opacity='1';"
			"const hf=s.sciMaxHull>0?s.sciHull/s.sciMaxHull:0;"
			"$('#hbar').style.width=Math.round(hf*100)+'%';"
			"$('#h').textContent=Math.round(s.sciHull)+' / '+Math.round(s.sciMaxHull);"
			"const sf=s.sciMaxShield>0?s.sciShield/s.sciMaxShield:0;"
			"$('#sbar').style.width=Math.round(sf*100)+'%';"
			"$('#sh').textContent=Math.round(s.sciShield)+' / '+Math.round(s.sciMaxShield);}"
			"else{r.style.opacity='.4';$('#hbar').style.width='0%';$('#sbar').style.width='0%';"
			"$('#h').textContent='—';$('#sh').textContent='—';}}");
		return MakePage(TEXT("SCIENCE"), TEXT("#37c"), Body, Script);
	}

	// Build a response with the given body + content type.
	TUniquePtr<FHttpServerResponse> MakeResponse(const FString& Body, const FString& ContentType)
	{
		return FHttpServerResponse::Create(Body, ContentType);
	}

	// Minimal JSON string escape (quotes + backslashes) for comms text built by hand.
	FString JsonEscape(const FString& In)
	{
		FString Out = In;
		Out.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
		Out.ReplaceInline(TEXT("\""), TEXT("\\\""));
		return Out;
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

	// Bind to all adapters (0.0.0.0), not just loopback, so other LAN devices can reach the
	// stations — the whole point of this server. Also enable address reuse so a later PIE
	// session can re-grab the port without waiting out the previous socket's TIME_WAIT. This
	// per-port override is read when the listener binds (below), so it must be set first.
	// NB: only takes effect when a *fresh* listener is created for the port — i.e. on the
	// first PIE after an editor launch (an already-listening loopback listener would be reused
	// as-is), so a full editor restart is needed the first time this lands.
	{
		TArray<FString> Overrides;
		Overrides.Add(FString::Printf(TEXT("(Port=%d,BindAddress=any,ReuseAddressAndPort=true)"), Port));
		GConfig->SetArray(TEXT("HTTPServer.Listeners"), TEXT("ListenerOverrides"), Overrides, GEngineIni);
	}

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

	// Station pages share one handler; the station id is a bound payload arg so each route
	// renders the right console (RelativePath is empty for exact-matched routes).
	auto BindStation = [this](const TCHAR* Path, int32 StationId)
	{
		FHttpRouteHandle Handle = Router->BindRoute(FHttpPath(Path), EHttpServerRequestVerbs::VERB_GET,
			FHttpRequestHandler::CreateUObject(this, &UStationServerSubsystem::HandleStationPage, StationId));
		if (Handle.IsValid()) { RouteHandles.Add(Handle); }
	};
	BindStation(TEXT("/helm"), 0);
	BindStation(TEXT("/weapons"), 1);
	BindStation(TEXT("/engineering"), 2);
	BindStation(TEXT("/science"), 3);
	// Command endpoints are GET (with query params): UE's HTTP server rejects POSTs that
	// lack a Content-Length header, which empty browser fetch() POSTs don't reliably send.
	// These are simple idempotent-ish console commands on a LAN, so GET is fine.
	Bind(TEXT("/api/state"), EHttpServerRequestVerbs::VERB_GET, &UStationServerSubsystem::HandleState);
	Bind(TEXT("/api/starmap"), EHttpServerRequestVerbs::VERB_GET, &UStationServerSubsystem::HandleStarmap);
	Bind(TEXT("/api/helm"), EHttpServerRequestVerbs::VERB_GET, &UStationServerSubsystem::HandleHelm);
	Bind(TEXT("/api/dock"), EHttpServerRequestVerbs::VERB_GET, &UStationServerSubsystem::HandleDock);
	Bind(TEXT("/api/warp"), EHttpServerRequestVerbs::VERB_GET, &UStationServerSubsystem::HandleWarp);
	Bind(TEXT("/api/buy"), EHttpServerRequestVerbs::VERB_GET, &UStationServerSubsystem::HandleBuy);
	Bind(TEXT("/api/ship"), EHttpServerRequestVerbs::VERB_GET, &UStationServerSubsystem::HandleShip);
	Bind(TEXT("/api/weapons"), EHttpServerRequestVerbs::VERB_GET, &UStationServerSubsystem::HandleWeapons);
	Bind(TEXT("/api/power"), EHttpServerRequestVerbs::VERB_GET, &UStationServerSubsystem::HandlePower);
	Bind(TEXT("/api/engineering"), EHttpServerRequestVerbs::VERB_GET, &UStationServerSubsystem::HandleEngineering);
	Bind(TEXT("/api/science"), EHttpServerRequestVerbs::VERB_GET, &UStationServerSubsystem::HandleScience);
	Bind(TEXT("/api/game"), EHttpServerRequestVerbs::VERB_GET, &UStationServerSubsystem::HandleGame);

	Http.StartAllListeners();

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

	// Deliberately NOT calling StopAllListeners(): it's global (it would also stop other
	// modules' listeners, e.g. the editor's on :3000), and tearing the socket down here only
	// to rebind it on the next PIE can fail while it lingers in TIME_WAIT. The listener stays
	// up with no routes (404s) between sessions; the next PIE re-grabs the same router and
	// rebinds. The HTTPServer module cleans the socket up on editor shutdown.

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

bool UStationServerSubsystem::HandleStationPage(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete, int32 StationId)
{
	FString Html;
	switch (StationId)
	{
	case 1:  Html = WeaponsPage(); break;
	case 2:  Html = EngineeringPage(); break;
	case 3:  Html = SciencePage(); break;
	default: Html = HelmPage(); break;
	}
	OnComplete(MakeResponse(Html, TEXT("text/html; charset=utf-8")));
	return true;
}

bool UStationServerSubsystem::HandleState(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	const ASpaceship* Ship = GetShip();
	const UShipMovementComponent* Move = Ship ? Ship->GetMovementComp() : nullptr;
	const UWeaponComponent* Weap = Ship ? Ship->GetWeaponComp() : nullptr;
	const UTorpedoLauncherComponent* Torp = Ship ? Ship->GetTorpedoComp() : nullptr;
	const UScienceComponent* Sci = Ship ? Ship->GetScienceComp() : nullptr;
	const UPowerComponent* Power = Ship ? Ship->GetPowerComp() : nullptr;
	const UHealthComponent* Health = Ship ? Ship->GetHealthComp() : nullptr;

	// Hostiles read out by radio callsign (e.g. "VIPER-2"); the friendly base by its name.
	auto DisplayName = [](const AActor* A) -> FString
	{
		if (const AEnemyShip* E = Cast<AEnemyShip>(A)) { return E->GetCallsign(); }
		if (const AStation* S = Cast<AStation>(A)) { return S->GetStationName(); }
		return A ? A->GetName() : TEXT("none");
	};

	const AActor* Target = Weap ? Weap->GetCurrentTarget() : nullptr;
	const FString TargetName = Target ? DisplayName(Target) : TEXT("none");

	const AActor* SciTarget = Sci ? Sci->GetScanTarget() : nullptr;
	const FString SciTargetName = SciTarget ? DisplayName(SciTarget) : TEXT("none");

	auto P = [Power](EShipSystem Sys) { return Power ? Power->GetSystemPower(Sys) : 0.f; };

	// Encounter outcome, so every console can show whether the game is live / won / lost.
	const TCHAR* PhaseStr = TEXT("playing");
	if (const UWorld* World = GetWorld())
	{
		if (const ASpaceGameMode* GM = World->GetAuthGameMode<ASpaceGameMode>())
		{
			const EGamePhase Phase = GM->GetPhase();
			PhaseStr = Phase == EGamePhase::Victory ? TEXT("victory")
				: Phase == EGamePhase::Defeat ? TEXT("defeat") : TEXT("playing");
		}
	}

	// Helm tactical map data: player world XY + heading, plus a blip per radar contact.
	const FVector PlayerLoc = Ship ? Ship->GetActorLocation() : FVector::ZeroVector;
	const float Heading = Ship ? Ship->GetActorRotation().Yaw : 0.f;

	FString Contacts;
	if (const UWorld* World = GetWorld())
	{
		for (TObjectIterator<URadarContactComponent> It; It; ++It)
		{
			const URadarContactComponent* C = *It;
			if (!IsValid(C) || C->GetWorld() != World) { continue; }
			const AActor* A = C->GetOwner();
			if (!A || A == Ship) { continue; }
			const FVector L = A->GetActorLocation();
			const FColor Col = C->BlipColor.ToFColor(true);
			const FString Label = DisplayName(A);
			if (!Contacts.IsEmpty()) { Contacts += TEXT(","); }
			Contacts += FString::Printf(TEXT("{\"x\":%.1f,\"y\":%.1f,\"color\":\"#%02x%02x%02x\",\"label\":\"%s\"}"),
				L.X, L.Y, Col.R, Col.G, Col.B, *Label);
		}
	}

	// Mission name + story comms log (Science console) from the mission subsystem.
	FString MissionName;
	FString Comms;
	bool bStaged = false;
	if (const UWorld* World = GetWorld())
	{
		if (const UMissionSubsystem* MS = World->GetSubsystem<UMissionSubsystem>())
		{
			MissionName = MS->GetMissionName();
			bStaged = MS->IsStaged();
			for (const FCommsMessage& M : MS->GetComms())
			{
				if (!Comms.IsEmpty()) { Comms += TEXT(","); }
				Comms += FString::Printf(TEXT("{\"sender\":\"%s\",\"text\":\"%s\"}"),
					*JsonEscape(M.Sender), *JsonEscape(M.Text));
			}
		}
	}

	// Campaign wallet (M19 progression) — credits/xp/rank, so any console can show standing.
	const USpaceGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<USpaceGameInstance>() : nullptr;

	// Drydock catalogue state (M19.3): per upgrade — owned tier, next cost + rank gate, and whether
	// it's buyable right now (docked + enough rank + credits + not maxed). Drives the Engineering store.
	FString Upgrades;
	{
		const int32 Credits = GI ? GI->GetCredits() : 0;
		const int32 Rank = GI ? GI->GetRank() : 1;
		const bool bDocked = Ship && Ship->IsDocked();
		for (const FUpgradeDef& U : UpgradeCatalogue::Get())
		{
			const int32 Tier = GI ? GI->GetUpgradeTier(U.Id) : 0;
			const bool bMaxed = Tier >= U.MaxTier;
			const int32 Cost = bMaxed ? 0 : UpgradeCatalogue::NextCost(U, Tier);
			const int32 RankReq = bMaxed ? 0 : UpgradeCatalogue::NextRankReq(U, Tier);
			const bool bAffordable = bDocked && !bMaxed && Rank >= RankReq && Credits >= Cost;
			if (!Upgrades.IsEmpty()) { Upgrades += TEXT(","); }
			Upgrades += FString::Printf(TEXT(
				"{\"id\":\"%s\",\"name\":\"%s\",\"unit\":\"%s\",\"tier\":%d,\"maxTier\":%d,"
				"\"mag\":%g,\"cost\":%d,\"rankReq\":%d,\"maxed\":%s,\"affordable\":%s}"),
				*U.Id.ToString(), *U.DisplayName, *U.Unit, Tier, U.MaxTier,
				U.MagnitudePerTier, Cost, RankReq,
				bMaxed ? TEXT("true") : TEXT("false"),
				bAffordable ? TEXT("true") : TEXT("false"));
		}
	}

	// Hangar roster (M19.4): per ship — owned, active, price + rank gate, and buyable right now.
	FString Ships;
	{
		const int32 Credits = GI ? GI->GetCredits() : 0;
		const int32 Rank = GI ? GI->GetRank() : 1;
		const bool bDocked = Ship && Ship->IsDocked();
		const EPlayerShipType Active = GI ? GI->GetPlayerShip() : EPlayerShipType::Interceptor;
		for (const FShipDef& S : ShipCatalogue::Get())
		{
			const bool bOwned = GI ? GI->OwnsShip(S.Type) : (S.Cost == 0);
			const bool bAffordable = bDocked && !bOwned && Rank >= S.RankReq && Credits >= S.Cost;
			if (!Ships.IsEmpty()) { Ships += TEXT(","); }
			Ships += FString::Printf(TEXT(
				"{\"id\":%d,\"name\":\"%s\",\"blurb\":\"%s\",\"owned\":%s,\"active\":%s,"
				"\"cost\":%d,\"rankReq\":%d,\"affordable\":%s}"),
				(int32)S.Type, *S.Name, *JsonEscape(S.Blurb),
				bOwned ? TEXT("true") : TEXT("false"),
				(S.Type == Active) ? TEXT("true") : TEXT("false"),
				S.Cost, S.RankReq,
				bAffordable ? TEXT("true") : TEXT("false"));
		}
	}

	// Hand-built JSON (avoids pulling in the Json module for this flat object).
	const FString Json = FString::Printf(TEXT(
		"{\"phase\":\"%s\",\"speed\":%.1f,\"throttle\":%.3f,\"maxSpeed\":%.1f,"
		"\"charge\":%.3f,\"target\":\"%s\",\"targetRange\":%.1f,\"inRange\":%s,\"inArc\":%s,"
		"\"inArcTorp\":%s,\"phaserArc\":%.0f,\"torpedoArc\":%.0f,"
		"\"power\":[%.3f,%.3f,%.3f],\"reactorLoad\":%.3f,\"reactorBudget\":%.3f,"
		"\"hull\":%.1f,\"maxHull\":%.1f,"
		"\"ammo\":%d,\"maxAmmo\":%d,\"torpedoReady\":%s,\"torpedoReload\":%.3f,"
		"\"sciTarget\":\"%s\",\"sciProgress\":%.3f,\"sciScanning\":%s,\"sciScanned\":%s,"
		"\"sciHull\":%.1f,\"sciMaxHull\":%.1f,\"sciShield\":%.1f,\"sciMaxShield\":%.1f,"
		"\"mission\":\"%s\",\"comms\":[%s],"
		"\"credits\":%d,\"xp\":%d,\"rank\":%d,"
		"\"docked\":%s,\"canDock\":%s,\"stationRange\":%.0f,\"staged\":%s,"
		"\"warpCharge\":%.3f,\"warpReady\":%s,\"upgrades\":[%s],\"ships\":[%s],"
		"\"heading\":%.1f,\"radarRange\":%.0f,\"px\":%.1f,\"py\":%.1f,\"contacts\":[%s]}"),
		PhaseStr,
		Move ? Move->GetSpeed() : 0.f,
		Move ? Move->GetThrottle() : 0.f,
		Move ? Move->GetEffectiveMaxSpeed() : 0.f,
		Weap ? Weap->GetCharge() : 0.f,
		*TargetName,
		Weap ? Weap->GetTargetRange() : -1.f,
		(Weap && Weap->IsTargetInRange()) ? TEXT("true") : TEXT("false"),
		(Weap && Weap->IsTargetInArc()) ? TEXT("true") : TEXT("false"),
		(Weap && Torp && Weap->IsTargetWithinArc(Torp->FireArcDeg)) ? TEXT("true") : TEXT("false"),
		Weap ? Weap->FireArcDeg : 0.f,
		Torp ? Torp->FireArcDeg : 0.f,
		P(EShipSystem::Engine), P(EShipSystem::Weapons), P(EShipSystem::Shields),
		Power ? Power->GetTotalPower() : 0.f,
		Power ? Power->ReactorBudget : 0.f,
		Health ? Health->GetHull() : 0.f,
		Health ? Health->GetMaxHull() : 0.f,
		Torp ? Torp->GetAmmo() : 0,
		Torp ? Torp->GetMaxAmmo() : 0,
		(Torp && Torp->IsReady()) ? TEXT("true") : TEXT("false"),
		Torp ? Torp->GetReloadFraction() : 1.f,
		*SciTargetName,
		Sci ? Sci->GetScanProgress() : 0.f,
		(Sci && Sci->IsScanning()) ? TEXT("true") : TEXT("false"),
		(Sci && Sci->IsScanned()) ? TEXT("true") : TEXT("false"),
		Sci ? Sci->GetTargetHull() : -1.f,
		Sci ? Sci->GetTargetMaxHull() : -1.f,
		Sci ? Sci->GetTargetShield() : -1.f,
		Sci ? Sci->GetTargetMaxShield() : -1.f,
		*MissionName, *Comms,
		GI ? GI->GetCredits() : 0, GI ? GI->GetXP() : 0, GI ? GI->GetRank() : 1,
		(Ship && Ship->IsDocked()) ? TEXT("true") : TEXT("false"),
		(Ship && Ship->CanDock()) ? TEXT("true") : TEXT("false"),
		Ship ? Ship->GetStationRange() : -1.f, bStaged ? TEXT("true") : TEXT("false"),
		Ship ? Ship->GetWarpCharge() : 0.f,
		(Ship && Ship->IsWarpReady()) ? TEXT("true") : TEXT("false"),
		*Upgrades, *Ships,
		Heading, HelmRadarRangeUU, PlayerLoc.X, PlayerLoc.Y, *Contacts);

	OnComplete(MakeResponse(Json, TEXT("application/json")));
	return true;
}

bool UStationServerSubsystem::HandleStarmap(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	// Sector starmap (M21): the campaign's systems laid out across the Veil frontier, with each marked
	// cleared / current / locked relative to the mission the crew is on.
	int32 Current = 0;
	if (const UWorld* World = GetWorld())
	{
		if (const UMissionSubsystem* MS = World->GetSubsystem<UMissionSubsystem>())
		{
			Current = MS->GetMissionIndex();
		}
	}

	FString Systems;
	const int32 Count = UMissionSubsystem::MissionCount();
	for (int32 i = 0; i < Count; ++i)
	{
		const FMissionDef Def = UMissionSubsystem::GetMissionDef(i);
		const TCHAR* Status = i < Current ? TEXT("cleared") : (i == Current ? TEXT("current") : TEXT("locked"));
		if (!Systems.IsEmpty()) { Systems += TEXT(","); }
		Systems += FString::Printf(TEXT("{\"name\":\"%s\",\"x\":%.3f,\"y\":%.3f,\"status\":\"%s\"}"),
			*JsonEscape(Def.Name), Def.MapX, Def.MapY, Status);
	}

	const FString Json = FString::Printf(
		TEXT("{\"sector\":\"THE VEIL FRONTIER\",\"current\":%d,\"systems\":[%s]}"), Current, *Systems);
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

bool UStationServerSubsystem::HandleDock(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	if (ASpaceship* Ship = GetShip())
	{
		const FString* Action = Request.QueryParams.Find(TEXT("action"));
		if (Action && *Action == TEXT("undock"))
		{
			Ship->Undock();
		}
		else // default: dock (no-op if out of range / moving)
		{
			Ship->Dock();
		}
	}
	OnComplete(MakeResponse(TEXT("ok"), TEXT("text/plain")));
	return true;
}

bool UStationServerSubsystem::HandleWarp(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	// Warp drive (M21): a charged FTL jump along the bow, triggered from the Helm.
	if (ASpaceship* Ship = GetShip())
	{
		Ship->Warp();
	}
	OnComplete(MakeResponse(TEXT("ok"), TEXT("text/plain")));
	return true;
}

bool UStationServerSubsystem::HandleBuy(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	// Drydock purchase (M19.3): only while docked. BuyUpgrade validates rank/credits/max + persists;
	// on success re-apply the loadout to the live ship so the upgrade takes effect immediately.
	ASpaceship* Ship = GetShip();
	const FString* Id = Request.QueryParams.Find(TEXT("id"));
	if (Ship && Ship->IsDocked() && Id)
	{
		if (USpaceGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<USpaceGameInstance>() : nullptr)
		{
			if (GI->BuyUpgrade(FName(**Id)))
			{
				Ship->RefreshLoadout();
			}
		}
	}
	OnComplete(MakeResponse(TEXT("ok"), TEXT("text/plain")));
	return true;
}

bool UStationServerSubsystem::HandleShip(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	// Hangar (M19.4): buy or switch the active ship — only while docked. id = EPlayerShipType value.
	ASpaceship* Ship = GetShip();
	const FString* Action = Request.QueryParams.Find(TEXT("action"));
	const FString* IdStr = Request.QueryParams.Find(TEXT("id"));
	if (Ship && Ship->IsDocked() && Action && IdStr)
	{
		const EPlayerShipType Type = (EPlayerShipType)FCString::Atoi(**IdStr);
		if (USpaceGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<USpaceGameInstance>() : nullptr)
		{
			if (*Action == TEXT("buy"))
			{
				GI->BuyShip(Type);
			}
			else if (*Action == TEXT("select"))
			{
				if (GI->SelectShip(Type)) { Ship->RefreshLoadout(); }
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
			else if (Action && *Action == TEXT("torpedo"))
			{
				if (UTorpedoLauncherComponent* Torp = Ship->GetTorpedoComp())
				{
					Torp->Fire();
				}
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

bool UStationServerSubsystem::HandleScience(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	if (ASpaceship* Ship = GetShip())
	{
		if (UScienceComponent* Sci = Ship->GetScienceComp())
		{
			const FString* Action = Request.QueryParams.Find(TEXT("action"));
			if (Action && *Action == TEXT("cycle"))
			{
				Sci->CycleTarget();
			}
			else if (Action && *Action == TEXT("scan"))
			{
				Sci->BeginScan();
			}
		}
	}
	OnComplete(MakeResponse(TEXT("ok"), TEXT("text/plain")));
	return true;
}

bool UStationServerSubsystem::HandleEngineering(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	// Each accepted weld restores a fixed chunk of hull, capped at MaxHull by RepairHull. A
	// minimum interval throttles it so a fast/scripted client can't weld the ship to full in
	// one frame — repairs stay paced like the on-screen sweep.
	constexpr float RepairPerHit = 8.f;
	constexpr double MinRepairInterval = 0.25; // seconds (sim time) between credited welds

	const FString* Action = Request.QueryParams.Find(TEXT("action"));
	if (Action && *Action == TEXT("repair"))
	{
		if (ASpaceship* Ship = GetShip())
		{
			if (UHealthComponent* Health = Ship->GetHealthComp())
			{
				const UWorld* World = GetWorld();
				const double Now = World ? World->GetTimeSeconds() : 0.0;
				if (Now - LastRepairTime >= MinRepairInterval)
				{
					LastRepairTime = Now;
					Health->RepairHull(RepairPerHit);
				}
			}
		}
	}
	OnComplete(MakeResponse(TEXT("ok"), TEXT("text/plain")));
	return true;
}

bool UStationServerSubsystem::HandleGame(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	// "restart" retries the current mission; "new" restarts the whole campaign from mission 0
	// (so it's meaningfully different from restart, and persists the reset).
	const FString* Action = Request.QueryParams.Find(TEXT("action"));
	if (UWorld* World = GetWorld())
	{
		if (Action && *Action == TEXT("new"))
		{
			if (USpaceGameInstance* GI = World->GetGameInstance<USpaceGameInstance>())
			{
				GI->ResetCampaign();
				GI->SaveCampaign();
			}
		}
		if (Action && (*Action == TEXT("restart") || *Action == TEXT("new")))
		{
			if (ASpaceGameMode* GM = World->GetAuthGameMode<ASpaceGameMode>())
			{
				GM->RestartEncounter();
			}
		}
		// "launch" ends the staging phase and spawns the mission's fleet (the crew starts the fight).
		if (Action && *Action == TEXT("launch"))
		{
			if (UMissionSubsystem* MS = World->GetSubsystem<UMissionSubsystem>())
			{
				MS->LaunchEncounter();
			}
		}
	}
	OnComplete(MakeResponse(TEXT("ok"), TEXT("text/plain")));
	return true;
}
