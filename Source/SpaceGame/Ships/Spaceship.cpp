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

	// Shared shape + material lookups (M13 — composited fighter with emissive hull).
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> HullMat(TEXT("/Game/Art/Materials/M_PlayerHull.M_PlayerHull"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlowMat(TEXT("/Game/Art/Materials/M_GlowCyan.M_GlowCyan"));

	// Main hull: engine cone, rotated so the apex points along +X (forward).
	ShipMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShipMesh"));
	ShipMesh->SetupAttachment(ShipRoot);
	ShipMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ShipMesh->SetCollisionObjectType(ECC_Pawn);
	// Engine cone's apex is local +Z; pitch +90 swings it to the actor's +X (forward).
	ShipMesh->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	ShipMesh->SetRelativeScale3D(FVector(0.85f, 0.85f, 2.4f)); // sleek, elongated nose
	if (ConeMesh.Succeeded()) { ShipMesh->SetStaticMesh(ConeMesh.Object); }
	if (HullMat.Succeeded())  { ShipMesh->SetMaterial(0, HullMat.Object); }

	// Swept wings: thin flat panels angled back from the mid-fuselage.
	WingLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WingLeft"));
	WingLeft->SetupAttachment(ShipRoot);
	WingLeft->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WingLeft->SetRelativeLocation(FVector(-25.f, -68.f, 0.f));
	WingLeft->SetRelativeRotation(FRotator(0.f, -22.f, 0.f));
	WingLeft->SetRelativeScale3D(FVector(0.9f, 1.5f, 0.07f));
	if (CubeMesh.Succeeded()) { WingLeft->SetStaticMesh(CubeMesh.Object); }
	if (HullMat.Succeeded())  { WingLeft->SetMaterial(0, HullMat.Object); }

	WingRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WingRight"));
	WingRight->SetupAttachment(ShipRoot);
	WingRight->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WingRight->SetRelativeLocation(FVector(-25.f, 68.f, 0.f));
	WingRight->SetRelativeRotation(FRotator(0.f, 22.f, 0.f));
	WingRight->SetRelativeScale3D(FVector(0.9f, 1.5f, 0.07f));
	if (CubeMesh.Succeeded()) { WingRight->SetStaticMesh(CubeMesh.Object); }
	if (HullMat.Succeeded())  { WingRight->SetMaterial(0, HullMat.Object); }

	// Glowing engine exhaust at the stern (cone base is at -X); cylinder axis along +X.
	EngineGlow = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EngineGlow"));
	EngineGlow->SetupAttachment(ShipRoot);
	EngineGlow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EngineGlow->SetRelativeLocation(FVector(-95.f, 0.f, 0.f));
	EngineGlow->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	EngineGlow->SetRelativeScale3D(FVector(0.55f, 0.55f, 0.5f));
	if (CylMesh.Succeeded()) { EngineGlow->SetStaticMesh(CylMesh.Object); }
	if (GlowMat.Succeeded()) { EngineGlow->SetMaterial(0, GlowMat.Object); }

	// 3rd-person follow camera. No collision test (space), gentle lag for feel.
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(ShipRoot);
	SpringArm->TargetArmLength = 900.f;
	SpringArm->SetRelativeRotation(FRotator(-12.f, 0.f, 0.f));
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

	UE_LOG(LogTemp, Log, TEXT("ASpaceship spawned: %s at %s"),
		*GetName(), *GetActorLocation().ToString());
}
