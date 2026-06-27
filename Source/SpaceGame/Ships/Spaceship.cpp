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
#include "Core/SpaceGameInstance.h"
#include "Core/UpgradeCatalogue.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "World/Station.h"

ASpaceship::ASpaceship()
{
	PrimaryActorTick.bCanEverTick = true;

	// Unrotated root: keeps actor forward (+X) decoupled from mesh visual rotation.
	ShipRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ShipRoot"));
	SetRootComponent(ShipRoot);

	// Imported player hull: Quaternius "Insurgent" fighter (CC0), blue palette (M13/assets).
	static ConstructorHelpers::FObjectFinder<UStaticMesh> HullMesh(TEXT("/Game/Art/Meshes/Insurgent.Insurgent"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> HullMat(TEXT("/Game/Art/Materials/M_Insurgent.M_Insurgent"));

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

	// Hits add camera trauma (M14 game-feel).
	if (HealthComp)
	{
		HealthComp->OnDamaged.AddDynamic(this, &ASpaceship::HandleDamaged);
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

	const TCHAR* MatPath = TEXT("/Game/Art/Materials/M_Insurgent.M_Insurgent");
	float Scale = 0.6f;

	// Reset the stats that the variant branch below doesn't explicitly set, so ApplyUpgrades()
	// always adds from a clean base — re-applying (drydock RefreshLoadout) stays idempotent.
	if (WeaponComp) { WeaponComp->FireArcDeg = 70.f; }
	if (HealthComp) { HealthComp->MaxShield = 50.f; }
	if (PowerComp)  { PowerComp->ReactorBudget = 3.0f; }

	if (Type == EPlayerShipType::Cruiser)
	{
		// Heavy: slower + less agile, but a much tougher hull and harder-hitting weapons.
		MatPath = TEXT("/Game/Art/Materials/M_PlayerHull.M_PlayerHull");
		Scale = 0.95f;
		if (MovementComp) { MovementComp->MaxSpeed = 1300.f; MovementComp->Acceleration = 900.f; MovementComp->MaxTurnRate = 42.f; }
		if (HealthComp)   { HealthComp->MaxHull = 160.f; }
		if (WeaponComp)   { WeaponComp->BeamDamage = 34.f; WeaponComp->BaseRechargeRate = 0.3f; }
		if (TorpedoComp)  { TorpedoComp->MaxAmmo = 6; }
	}
	else
	{
		// Interceptor: fast + agile, lighter hull, quicker (lighter) beam.
		Scale = 0.6f;
		if (MovementComp) { MovementComp->MaxSpeed = 2100.f; MovementComp->Acceleration = 1500.f; MovementComp->MaxTurnRate = 75.f; }
		if (HealthComp)   { HealthComp->MaxHull = 80.f; }
		if (WeaponComp)   { WeaponComp->BeamDamage = 20.f; WeaponComp->BaseRechargeRate = 0.55f; }
		if (TorpedoComp)  { TorpedoComp->MaxAmmo = 3; }
	}

	if (ShipMesh)
	{
		if (UMaterialInterface* Mat = Cast<UMaterialInterface>(
			StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, MatPath)))
		{
			ShipMesh->SetMaterial(0, Mat);
		}
		ShipMesh->SetRelativeScale3D(FVector(Scale));
	}

	// Layer purchased drydock upgrades on top of the base variant stats.
	ApplyUpgrades();

	UE_LOG(LogTemp, Log, TEXT("[Ship] Variant: %s"),
		Type == EPlayerShipType::Cruiser ? TEXT("Cruiser") : TEXT("Interceptor"));
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
