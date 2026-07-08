// SpaceGame — bridge simulator. Ship pawn implementation.

#include "Ships/Spaceship.h"

#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/HealthComponent.h"
#include "Components/PowerComponent.h"
#include "Components/ShipMovementComponent.h"
#include "Components/ScienceComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TorpedoLauncherComponent.h"
#include "Components/WeaponComponent.h"
#include "Core/ShipCatalogue.h"
#include "Core/SpaceGameInstance.h"
#include "Core/UpgradeCatalogue.h"
#include "Engine/StaticMesh.h"
#include "FX/BeamFx.h"
#include "FX/Debris.h"
#include "FX/ExplosionFx.h"
#include "Ships/EnemyShip.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "World/Station.h"
#include "World/WorldLandmark.h"

ASpaceship::ASpaceship()
{
	PrimaryActorTick.bCanEverTick = true;

	// Unrotated root: keeps actor forward (+X) decoupled from mesh visual rotation.
	ShipRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ShipRoot"));
	SetRootComponent(ShipRoot);

	// Imported player hull: Quaternius "Insurgent" fighter (CC0), blue palette (M13/assets).
	static ConstructorHelpers::FObjectFinder<UStaticMesh> HullMesh(TEXT("/Game/Art/Meshes/Insurgent.Insurgent"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> HullMat(TEXT("/Game/Art/Materials/M_Insurgent.M_Insurgent"));

	// Cyan flash for the warp-jump FX + shield-impact sparks (M21/M22).
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WarpFx(TEXT("/Game/Art/Materials/M_GlowCyan.M_GlowCyan"));
	if (WarpFx.Succeeded()) { WarpFxMaterial = WarpFx.Object; }

	// Orange sparks for hull-on-hull collisions (M22).
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Spark(TEXT("/Game/Art/Materials/M_GlowOrange.M_GlowOrange"));
	if (Spark.Succeeded()) { HullSparkMaterial = Spark.Object; }

	// Main hull. The model's length runs along its local Y; yaw -90 swings the nose to
	// the actor's +X (forward). Scaled down from the asset's ~1100uu length.
	ShipMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShipMesh"));
	ShipMesh->SetupAttachment(ShipRoot);
	ShipMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ShipMesh->SetCollisionObjectType(ECC_Pawn);
	ShipMesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	ShipMesh->SetRelativeScale3D(FVector(0.6f));
	if (HullMesh.Succeeded()) { ShipMesh->SetStaticMesh(HullMesh.Object); }
	if (HullMat.Succeeded())  { ShipMesh->SetMaterial(0, HullMat.Object); }

	// 3rd-person follow camera. No collision test (space), gentle lag for feel.
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(ShipRoot);
	SpringArm->TargetArmLength = 1600.f;
	SpringArm->SetRelativeRotation(FRotator(-14.f, 0.f, 0.f));
	SpringArm->bDoCollisionTest = false;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 5.f;
	SpringArm->bEnableCameraRotationLag = true;
	SpringArm->CameraRotationLagSpeed = 6.f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);

	// Impulse movement simulation (throttle/turn).
	MovementComp = CreateDefaultSubobject<UShipMovementComponent>(TEXT("MovementComp"));

	// Engineering power model (engine power scales MaxSpeed; weapons/shields at M8/M9).
	PowerComp = CreateDefaultSubobject<UPowerComponent>(TEXT("PowerComp"));

	// Forward beam weapon (recharge scaled by Weapons power, M8).
	WeaponComp = CreateDefaultSubobject<UWeaponComponent>(TEXT("WeaponComp"));

	// Limited-ammo torpedo launcher (M17); shares the beam's locked target.
	TorpedoComp = CreateDefaultSubobject<UTorpedoLauncherComponent>(TEXT("TorpedoComp"));

	// Science sensors (M17): scans enemy contacts to reveal hull/shield.
	ScienceComp = CreateDefaultSubobject<UScienceComponent>(TEXT("ScienceComp"));

	// Hull + shield-power mitigation (M11). The player's "shields" are the engineering
	// Shields-power mitigation (D11), so the absorb pool is zero — damage hits hull,
	// softened by shield power. Enemy beams drain this; 0 hull triggers the defeat screen.
	HealthComp = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComp"));
	HealthComp->MaxHull = 100.f;
	HealthComp->MaxShield = 0.f;

	// Hull-hit SFX (CC0).
	static ConstructorHelpers::FObjectFinder<USoundBase> Hit(TEXT("/Game/Audio/S_Hit.S_Hit"));
	if (Hit.Succeeded()) { HitSound = Hit.Object; }

	// Looping engine-hum bed (M14). Auto-plays at idle volume; Tick rides volume + pitch
	// on throttle so the drive audibly spools up under power.
	static ConstructorHelpers::FObjectFinder<USoundBase> Hum(TEXT("/Game/Audio/S_EngineHum.S_EngineHum"));
	EngineAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("EngineAudio"));
	EngineAudio->SetupAttachment(ShipRoot);
	EngineAudio->bAutoActivate = true;
	EngineAudio->VolumeMultiplier = 0.12f;
	if (Hum.Succeeded()) { EngineAudio->SetSound(Hum.Object); }

	// Looping low-hull klaxon (M14). Stays silent until UpdateAmbientAudio plays it as
	// hull drops past LowHullFraction.
	static ConstructorHelpers::FObjectFinder<USoundBase> Alarm(TEXT("/Game/Audio/S_Alarm.S_Alarm"));
	AlarmAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("AlarmAudio"));
	AlarmAudio->SetupAttachment(ShipRoot);
	AlarmAudio->bAutoActivate = false;
	AlarmAudio->VolumeMultiplier = 0.5f;
	if (Alarm.Succeeded()) { AlarmAudio->SetSound(Alarm.Object); }

	// Possess automatically so PIE shows the ship through its follow camera.
	AutoPossessPlayer = EAutoReceiveInput::Player0;
}

void ASpaceship::BeginPlay()
{
	// Apply the campaign's chosen variant before Super, so the components init their pools/ammo
	// from the variant's Max values (Health Hull, Torpedo ammo).
	ApplyShipPreset();

	Super::BeginPlay();

	// Hits add camera trauma (M14 game-feel); death blows the hull apart (M24).
	if (HealthComp)
	{
		HealthComp->OnDamaged.AddDynamic(this, &ASpaceship::HandleDamaged);
		HealthComp->OnDeath.AddDynamic(this, &ASpaceship::HandleShipDestroyed);
	}

	UE_LOG(LogTemp, Log, TEXT("ASpaceship spawned: %s at %s"),
		*GetName(), *GetActorLocation().ToString());
}

void ASpaceship::ApplyShipPreset()
{
	EPlayerShipType Type = EPlayerShipType::Interceptor;
	if (const USpaceGameInstance* GI = GetGameInstance<USpaceGameInstance>())
	{
		Type = GI->GetPlayerShip();
	}

	// Base hull stats come from the ship roster (M19.4). Falls back to the Interceptor def.
	const FShipDef* Def = ShipCatalogue::Find(Type);
	if (!Def) { Def = ShipCatalogue::Find(EPlayerShipType::Interceptor); }
	if (!Def) { return; }

	// Reset the stats not carried in FShipDef, so ApplyUpgrades() always adds from a clean base —
	// re-applying (drydock RefreshLoadout / ship switch) stays idempotent.
	if (WeaponComp) { WeaponComp->FireArcDeg = 70.f; }
	if (HealthComp) { HealthComp->MaxShield = 50.f; }
	if (PowerComp)  { PowerComp->ReactorBudget = 3.0f; }

	if (MovementComp) { MovementComp->MaxSpeed = Def->MaxSpeed; MovementComp->Acceleration = Def->Acceleration; MovementComp->MaxTurnRate = Def->TurnRate; }
	if (HealthComp)   { HealthComp->MaxHull = Def->MaxHull; }
	if (WeaponComp)   { WeaponComp->BeamDamage = Def->BeamDamage; WeaponComp->BaseRechargeRate = Def->BeamRecharge; }
	if (TorpedoComp)  { TorpedoComp->MaxAmmo = Def->TorpedoAmmo; }

	if (ShipMesh)
	{
		if (UStaticMesh* Hull = Cast<UStaticMesh>(
			StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *Def->MeshPath)))
		{
			ShipMesh->SetStaticMesh(Hull);
		}
		if (UMaterialInterface* Mat = Cast<UMaterialInterface>(
			StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *Def->MatPath)))
		{
			ShipMesh->SetMaterial(0, Mat);
		}
		ShipMesh->SetRelativeScale3D(FVector(Def->Scale));
	}

	// Layer purchased drydock upgrades on top of the base hull stats.
	ApplyUpgrades();

	UE_LOG(LogTemp, Log, TEXT("[Ship] Variant: %s"), *Def->Name);
}

void ASpaceship::ApplyUpgrades()
{
	const USpaceGameInstance* GI = GetGameInstance<USpaceGameInstance>();
	if (!GI) { return; }

	for (const FUpgradeDef& U : UpgradeCatalogue::Get())
	{
		const int32 Tier = GI->GetUpgradeTier(U.Id);
		if (Tier <= 0) { continue; }
		const float Add = U.MagnitudePerTier * Tier;
		switch (U.Stat)
		{
		case EUpgradeStat::BeamDamage:    if (WeaponComp)  { WeaponComp->BeamDamage += Add; } break;
		case EUpgradeStat::BeamRecharge:  if (WeaponComp)  { WeaponComp->BaseRechargeRate += Add; } break;
		case EUpgradeStat::FireArc:       if (WeaponComp)  { WeaponComp->FireArcDeg += Add; } break;
		case EUpgradeStat::MaxHull:       if (HealthComp)  { HealthComp->MaxHull += Add; } break;
		case EUpgradeStat::MaxShield:     if (HealthComp)  { HealthComp->MaxShield += Add; } break;
		case EUpgradeStat::TorpedoAmmo:   if (TorpedoComp) { TorpedoComp->MaxAmmo += FMath::RoundToInt(Add); } break;
		case EUpgradeStat::ReactorBudget: if (PowerComp)   { PowerComp->ReactorBudget += Add; } break;
		}
	}
}

void ASpaceship::RefreshLoadout()
{
	// Re-derive base variant + upgrades, then top the pools up to the new maxima (drydock is safe).
	ApplyShipPreset();
	if (HealthComp)  { HealthComp->ResetPools(); }
	if (TorpedoComp) { TorpedoComp->Resupply(); }
	UE_LOG(LogTemp, Log, TEXT("[Ship] Loadout refreshed from drydock upgrades"));
}

void ASpaceship::AddCameraTrauma(float Amount)
{
	CameraTrauma = FMath::Clamp(CameraTrauma + Amount, 0.f, 1.f);
}

AStation* ASpaceship::NearestStation() const
{
	const UWorld* World = GetWorld();
	if (!World) { return nullptr; }

	TArray<AActor*> Stations;
	UGameplayStatics::GetAllActorsOfClass(World, AStation::StaticClass(), Stations);
	AStation* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	const FVector Loc = GetActorLocation();
	for (AActor* A : Stations)
	{
		AStation* S = Cast<AStation>(A);
		if (!S) { continue; }
		const float D = FVector::DistSquared(Loc, S->GetActorLocation());
		if (D < BestDistSq) { BestDistSq = D; Best = S; }
	}
	return Best;
}

float ASpaceship::GetStationRange() const
{
	const AStation* S = NearestStation();
	return S ? FVector::Dist(GetActorLocation(), S->GetActorLocation()) : -1.f;
}

bool ASpaceship::CanDock() const
{
	if (bDocked) { return false; }
	const AStation* S = NearestStation();
	if (!S) { return false; }
	if (FVector::Dist(GetActorLocation(), S->GetActorLocation()) > S->GetDockRange()) { return false; }
	// Must be nearly stopped to dock (you can't slam into a starbase at full impulse).
	const float Speed = MovementComp ? FMath::Abs(MovementComp->GetSpeed()) : 0.f;
	return Speed <= DockMaxSpeed;
}

bool ASpaceship::Dock()
{
	if (!CanDock()) { return false; }
	bDocked = true;

	// Freeze the ship and hand it nothing to do, become combat-safe, then repair + restock.
	if (MovementComp) { MovementComp->SetInputLocked(true); }
	if (HealthComp)
	{
		HealthComp->SetInvulnerable(true);
		HealthComp->ResetPools(); // refill hull + shield
	}
	if (TorpedoComp) { TorpedoComp->Resupply(); }

	UE_LOG(LogTemp, Log, TEXT("[Dock] Docked — repaired + restocked, combat-safe"));
	return true;
}

void ASpaceship::Undock()
{
	if (!bDocked) { return; }
	bDocked = false;
	if (MovementComp) { MovementComp->SetInputLocked(false); }
	if (HealthComp)   { HealthComp->SetInvulnerable(false); }
	UE_LOG(LogTemp, Log, TEXT("[Dock] Undocked — helm control restored"));
}

void ASpaceship::HandleCollisions(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	if (!World || bDocked) { return; } // no ramming while docked at the starbase

	const FVector MyLoc = GetActorLocation();
	const float Speed = MovementComp ? FMath::Abs(MovementComp->GetSpeed()) : 0.f;
	const float MaxSpeed = MovementComp ? FMath::Max(1.f, MovementComp->MaxSpeed) : 2100.f;
	// Ram damage scales from 0.5x (standstill nudge) up to 1.5x at full impulse.
	const float Impact = RamDamage * (0.5f + FMath::Clamp(Speed / MaxSpeed, 0.f, 1.f));
	const bool bMyShield = HealthComp && HealthComp->GetShield() > 0.f;

	TArray<AActor*> Enemies;
	UGameplayStatics::GetAllActorsOfClass(World, AEnemyShip::StaticClass(), Enemies);

	TSet<TWeakObjectPtr<AActor>> StillTouching;
	for (AActor* A : Enemies)
	{
		AEnemyShip* Enemy = Cast<AEnemyShip>(A);
		UHealthComponent* EnemyHealth = Enemy ? Enemy->GetHealthComp() : nullptr;
		if (!EnemyHealth || !EnemyHealth->IsAlive()) { continue; }

		// Shields enlarge the hitbox: each shielded ship's bubble juts past the hull, so contact
		// happens sooner and the impact is read as a shield flare rather than a hull spark.
		const bool bEnemyShield = EnemyHealth->GetShield() > 0.f;
		const float ContactDist = CollisionRadius
			+ (bMyShield ? ShieldRadiusBonus : 0.f)
			+ (bEnemyShield ? ShieldRadiusBonus : 0.f);

		FVector ToEnemy = Enemy->GetActorLocation() - MyLoc;
		const float Dist = ToEnemy.Size();
		if (Dist >= ContactDist || Dist < 1.f) { continue; }

		StillTouching.Add(Enemy);
		if (TouchingActors.Contains(Enemy)) { continue; } // already counted this contact

		// New contact: both hulls take ram damage (a hard, mutual hit), then knock apart.
		EnemyHealth->ApplyDamage(Impact);
		if (HealthComp) { HealthComp->ApplyDamage(Impact * RamSelfFraction); }
		AddCameraTrauma(0.7f);

		const FVector Normal = ToEnemy / Dist;
		const float Push = (ContactDist - Dist) * 0.5f + 250.f;
		AddActorWorldOffset(-Normal * Push, false, nullptr, ETeleportType::TeleportPhysics);
		Enemy->AddActorWorldOffset(Normal * Push, false, nullptr, ETeleportType::TeleportPhysics);

		// Sparks on the shield bubble (cyan) or bare hull (orange), not an explosion.
		const bool bShieldHit = bEnemyShield || bMyShield;
		SpawnImpactSparks(MyLoc + Normal * (Dist * 0.5f), bShieldHit);
		UE_LOG(LogTemp, Log, TEXT("[Collision] Rammed %s — %.0f dmg (self %.0f) at %.0f uu/s%s"),
			*Enemy->GetName(), Impact, Impact * RamSelfFraction, Speed, bShieldHit ? TEXT(" [shield]") : TEXT(""));
	}

	// Solid celestial bodies: a planet or sun blocks the ship — clamp it to the surface each tick so it
	// can't fly through, and cut the impulse so it doesn't keep grinding in. (Shares the not-docked guard.)
	TArray<AActor*> Bodies;
	UGameplayStatics::GetAllActorsOfClass(World, AWorldLandmark::StaticClass(), Bodies);
	for (AActor* A : Bodies)
	{
		AWorldLandmark* Body = Cast<AWorldLandmark>(A);
		if (!Body) { continue; }
		const float Surface = Body->GetBodyRadius() + BodyClearance;
		FVector ToShip = GetActorLocation() - Body->GetActorLocation();
		const float Dist = ToShip.Size();
		if (Dist >= Surface || Dist < 1.f) { continue; }

		StillTouching.Add(Body);
		const FVector Normal = ToShip / Dist;
		AddActorWorldOffset(Normal * (Surface - Dist), false, nullptr, ETeleportType::TeleportPhysics);
		if (MovementComp) { MovementComp->SetThrottle(0.f); } // engines can't push you through a planet

		if (!TouchingActors.Contains(Body))
		{
			AddCameraTrauma(0.6f);
			UE_LOG(LogTemp, Log, TEXT("[Collision] Struck %s — halted at the surface"), *Body->GetLandmarkName());
		}
	}

	TouchingActors = MoveTemp(StillTouching);
}

void ASpaceship::SpawnImpactSparks(const FVector& Location, bool bShieldHit)
{
	UWorld* World = GetWorld();
	if (!World) { return; }
	UMaterialInterface* Mat = bShieldHit ? WarpFxMaterial : HullSparkMaterial;

	// A short shower of streaks radiating from the contact point — reads as an impact, not a blast.
	const int32 Count = bShieldHit ? 10 : 7;
	for (int32 i = 0; i < Count; ++i)
	{
		const FVector Dir = FMath::VRand();
		const float Len = FMath::FRandRange(180.f, 420.f);
		ABeamFx::Spawn(World, Location, Location + Dir * Len, Mat,
			FMath::FRandRange(7.f, 13.f), FMath::FRandRange(0.12f, 0.22f));
	}
}

bool ASpaceship::Warp()
{
	if (!IsWarpReady())
	{
		return false;
	}

	const FVector From = GetActorLocation();
	const FVector Delta = GetActorForwardVector() * WarpDistance;
	AddActorWorldOffset(Delta, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
	WarpCharge = 0.f;
	AddCameraTrauma(0.75f);

	// Warp flash at the departure and arrival points.
	if (WarpFxMaterial)
	{
		AExplosionFx::Spawn(GetWorld(), From, 600.f, WarpFxMaterial, 0.45f, false);
		AExplosionFx::Spawn(GetWorld(), GetActorLocation(), 600.f, WarpFxMaterial, 0.45f, false);
	}
	UE_LOG(LogTemp, Log, TEXT("[Warp] Jumped %.0f uu along bow"), WarpDistance);
	return true;
}

bool ASpaceship::WarpToObjective(FVector Target)
{
	if (!IsWarpReady())
	{
		return false;
	}

	const FVector From = GetActorLocation();
	FVector Flat = Target - From;
	Flat.Z = 0.f;
	const float Dist = Flat.Size();
	const FVector Dir = (Dist > 1.f) ? Flat / Dist : GetActorForwardVector();

	// Turn the bow toward the objective (yaw only — the ship's momentum follows its new heading).
	FRotator Face = Dir.Rotation();
	Face.Pitch = 0.f;
	Face.Roll = 0.f;
	SetActorRotation(Face, ETeleportType::TeleportPhysics);

	// Jump toward it without overshooting — leave a standoff so we arrive near, not on top of, it.
	// The standoff scales with the destination body: a fixed 4000 uu would land inside a big body's
	// surface clamp (the Ember sun's radius is ~12500), so clear its radius plus a margin (M24).
	float Standoff = 4000.f;
	TArray<AActor*> Bodies;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWorldLandmark::StaticClass(), Bodies);
	for (AActor* A : Bodies)
	{
		const AWorldLandmark* Body = Cast<AWorldLandmark>(A);
		if (Body && FVector::Dist(Body->GetActorLocation(), Target) <= Body->GetBodyRadius() + 2000.f)
		{
			Standoff = FMath::Max(Standoff, Body->GetBodyRadius() + 3000.f);
		}
	}
	const float Jump = FMath::Clamp(Dist - Standoff, 0.f, WarpDistance);
	const FVector NewLoc = From + Dir * Jump;
	SetActorLocation(NewLoc, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
	WarpCharge = 0.f;
	AddCameraTrauma(0.75f);

	if (WarpFxMaterial)
	{
		AExplosionFx::Spawn(GetWorld(), From, 600.f, WarpFxMaterial, 0.45f, false);
		AExplosionFx::Spawn(GetWorld(), GetActorLocation(), 600.f, WarpFxMaterial, 0.45f, false);
	}
	UE_LOG(LogTemp, Log, TEXT("[Warp] Course laid — jumped %.0f uu toward objective (%.0f remaining)"),
		Jump, FMath::Max(0.f, Dist - Jump));
	return true;
}

void ASpaceship::HandleShipDestroyed(AActor* /*DeadActor*/)
{
	UWorld* World = GetWorld();
	const FVector Loc = GetActorLocation();

	// Same multi-burst + wreckage treatment enemy kills get (M14), so the player's own death
	// reads as an event instead of a freeze-frame straight into the defeat menu.
	AExplosionFx::Spawn(World, Loc, 1100.f, HullSparkMaterial, 0.7f);
	for (int32 i = 0; i < 5; ++i)
	{
		const FVector Offset = FMath::VRand() * FMath::FRandRange(220.f, 700.f);
		AExplosionFx::Spawn(World, Loc + Offset, FMath::FRandRange(260.f, 500.f),
			HullSparkMaterial, FMath::FRandRange(0.3f, 0.55f), false);
	}
	const float ShipScale = ShipMesh ? ShipMesh->GetRelativeScale3D().X : 0.6f;
	for (int32 i = 0; i < 6; ++i)
	{
		const FVector Vel = FMath::VRand() * FMath::FRandRange(350.f, 950.f);
		ADebris::Spawn(World, Loc + Vel.GetSafeNormal() * 120.f, Vel, HullSparkMaterial,
			ShipScale * 0.7f, FMath::FRandRange(8.f, 14.f));
	}

	// The hull is gone: hide it, kill the drive audio, and freeze the helm. The pawn itself
	// stays alive so the follow camera keeps framing the wreck during the defeat beat.
	if (ShipMesh)
	{
		ShipMesh->SetVisibility(false, true);
		ShipMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (MovementComp)
	{
		MovementComp->SetThrottle(0.f);
		MovementComp->SetTurn(0.f);
		MovementComp->SetInputLocked(true);
	}
	if (EngineAudio) { EngineAudio->Stop(); }
	AddCameraTrauma(1.f);

	UE_LOG(LogTemp, Log, TEXT("[Ship] Destroyed — hull blown apart, helm frozen"));
}

void ASpaceship::HandleDamaged(float EffectiveDamage, float HullRemaining)
{
	AddCameraTrauma(EffectiveDamage * HitTraumaPerDamage);
	if (HitSound)
	{
		// Slight per-hit pitch jitter so rapid hits don't sound machine-stamped (M14).
		UGameplayStatics::PlaySound2D(this, HitSound, 1.f, FMath::FRandRange(0.92f, 1.08f));
	}
}

void ASpaceship::UpdateAmbientAudio()
{
	// Engine-hum bed rides throttle: quiet idle → fuller + slightly higher-pitched under power.
	if (EngineAudio)
	{
		const float Throttle = MovementComp ? FMath::Clamp(FMath::Abs(MovementComp->GetThrottle()), 0.f, 1.f) : 0.f;
		EngineAudio->SetVolumeMultiplier(FMath::Lerp(0.12f, 0.5f, Throttle));
		EngineAudio->SetPitchMultiplier(FMath::Lerp(0.9f, 1.25f, Throttle));
	}

	// Low-hull klaxon: edge-triggered play/stop as hull crosses LowHullFraction.
	if (AlarmAudio && HealthComp)
	{
		const float MaxHull = HealthComp->GetMaxHull();
		const float Frac = MaxHull > 0.f ? HealthComp->GetHull() / MaxHull : 0.f;
		const bool bShouldAlarm = HealthComp->IsAlive() && Frac <= LowHullFraction;
		if (bShouldAlarm && !bAlarmActive)
		{
			AlarmAudio->Play();
			bAlarmActive = true;
		}
		else if (!bShouldAlarm && bAlarmActive)
		{
			AlarmAudio->Stop();
			bAlarmActive = false;
		}
	}
}

void ASpaceship::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateAmbientAudio();

	// Warp drive trickle-charges over time, faster with engine power routed (Engineering synergy).
	if (!bDocked)
	{
		const float EnginePower = PowerComp ? PowerComp->GetSystemPower(EShipSystem::Engine) : 1.f;
		WarpCharge = FMath::Clamp(WarpCharge + WarpChargeRate * EnginePower * DeltaSeconds, 0.f, 1.f);
	}

	HandleCollisions(DeltaSeconds);

	if (!FollowCamera)
	{
		return;
	}

	if (CameraTrauma > 0.f)
	{
		// Shake scales with trauma² for a punchy, fast-settling feel (Vlambeer "trauma").
		const float Shake = CameraTrauma * CameraTrauma;
		const FRotator Offset(
			MaxShakePitch * Shake * FMath::FRandRange(-1.f, 1.f),
			MaxShakeYaw   * Shake * FMath::FRandRange(-1.f, 1.f),
			MaxShakeRoll  * Shake * FMath::FRandRange(-1.f, 1.f));
		FollowCamera->SetRelativeRotation(Offset);
		CameraTrauma = FMath::Max(0.f, CameraTrauma - TraumaDecayPerSec * DeltaSeconds);
	}
	else if (!FollowCamera->GetRelativeRotation().IsNearlyZero())
	{
		// Settle back to neutral once trauma is spent.
		FollowCamera->SetRelativeRotation(FRotator::ZeroRotator);
	}
}
