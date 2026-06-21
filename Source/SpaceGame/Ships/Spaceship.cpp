// SpaceGame — bridge simulator. Ship pawn implementation.

#include "Ships/Spaceship.h"

#include "Camera/CameraComponent.h"
#include "Components/HealthComponent.h"
#include "Components/PowerComponent.h"
#include "Components/ShipMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WeaponComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInterface.h"
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
}

void ASpaceship::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

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
