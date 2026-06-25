// SpaceGame — bridge simulator. Ship pawn implementation.

#include "Ships/Spaceship.h"

#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/HealthComponent.h"
#include "Components/PowerComponent.h"
#include "Components/ShipMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WeaponComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

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
	Super::BeginPlay();

	// Hits add camera trauma (M14 game-feel).
	if (HealthComp)
	{
		HealthComp->OnDamaged.AddDynamic(this, &ASpaceship::HandleDamaged);
	}

	UE_LOG(LogTemp, Log, TEXT("ASpaceship spawned: %s at %s"),
		*GetName(), *GetActorLocation().ToString());
}

void ASpaceship::AddCameraTrauma(float Amount)
{
	CameraTrauma = FMath::Clamp(CameraTrauma + Amount, 0.f, 1.f);
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
