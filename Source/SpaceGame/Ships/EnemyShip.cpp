// SpaceGame — bridge simulator. Enemy ship pawn + simple AI implementation.

#include "Ships/EnemyShip.h"

#include "Components/HealthComponent.h"
#include "Components/RadarContactComponent.h"
#include "Components/StaticMeshComponent.h"
#include "FX/BeamFx.h"
#include "FX/ExplosionFx.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

AEnemyShip::AEnemyShip()
{
	PrimaryActorTick.bCanEverTick = true;

	// No AIController: the AI lives in Tick (simple is fine for the vertical slice).
	AutoPossessAI = EAutoPossessAI::Disabled;

	ShipRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ShipRoot"));
	SetRootComponent(ShipRoot);

	// Shared shape + material lookups (M13 — composited cruiser with emissive hull).
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> HullMat(TEXT("/Game/Art/Materials/M_EnemyHull.M_EnemyHull"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RedGlow(TEXT("/Game/Art/Materials/M_GlowRed.M_GlowRed"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> OrangeGlow(TEXT("/Game/Art/Materials/M_GlowOrange.M_GlowOrange"));

	// Main hull: a wide blocky cruiser body.
	ShipMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShipMesh"));
	ShipMesh->SetupAttachment(ShipRoot);
	ShipMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ShipMesh->SetCollisionObjectType(ECC_Pawn);
	ShipMesh->SetRelativeScale3D(FVector(2.4f, 1.3f, 0.7f));
	if (CubeMesh.Succeeded()) { ShipMesh->SetStaticMesh(CubeMesh.Object); }
	if (HullMat.Succeeded())  { ShipMesh->SetMaterial(0, HullMat.Object); }

	// Twin forward prongs jutting toward the bow (+X).
	auto MakeProng = [&](const TCHAR* Name, float Y) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* P = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		P->SetupAttachment(ShipRoot);
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		P->SetRelativeLocation(FVector(150.f, Y, 0.f));
		P->SetRelativeScale3D(FVector(1.7f, 0.28f, 0.28f));
		if (CubeMesh.Succeeded()) { P->SetStaticMesh(CubeMesh.Object); }
		if (HullMat.Succeeded())  { P->SetMaterial(0, HullMat.Object); }
		return P;
	};
	ProngLeft  = MakeProng(TEXT("ProngLeft"),  -55.f);
	ProngRight = MakeProng(TEXT("ProngRight"),  55.f);

	// Glowing red sensor eye at the bow.
	SensorEye = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SensorEye"));
	SensorEye->SetupAttachment(ShipRoot);
	SensorEye->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SensorEye->SetRelativeLocation(FVector(120.f, 0.f, 18.f));
	SensorEye->SetRelativeScale3D(FVector(0.45f, 0.45f, 0.45f));
	if (SphereMesh.Succeeded()) { SensorEye->SetStaticMesh(SphereMesh.Object); }
	if (RedGlow.Succeeded())    { SensorEye->SetMaterial(0, RedGlow.Object); }

	// Engine glow at the stern (-X); cylinder axis along +X.
	EngineGlow = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EngineGlow"));
	EngineGlow->SetupAttachment(ShipRoot);
	EngineGlow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EngineGlow->SetRelativeLocation(FVector(-110.f, 0.f, 0.f));
	EngineGlow->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	EngineGlow->SetRelativeScale3D(FVector(0.7f, 0.7f, 0.5f));
	if (CylMesh.Succeeded())    { EngineGlow->SetStaticMesh(CylMesh.Object); }
	if (OrangeGlow.Succeeded()) { EngineGlow->SetMaterial(0, OrangeGlow.Object); }

	// Beam + explosion FX tint (M13).
	if (OrangeGlow.Succeeded()) { FxMaterial = OrangeGlow.Object; }

	// Radar blip (hostile red default) + makes this a valid player weapon target.
	RadarContact = CreateDefaultSubobject<URadarContactComponent>(TEXT("RadarContact"));

	// Hull + shields; the player's beam drains this at M10.
	HealthComp = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComp"));
}

void AEnemyShip::BeginPlay()
{
	Super::BeginPlay();

	FireCooldown = FireInterval;

	// Despawn + explode when the player's beam drains our hull to zero (M10).
	if (HealthComp)
	{
		HealthComp->OnDeath.AddDynamic(this, &AEnemyShip::HandleDeath);
	}

	UE_LOG(LogTemp, Log, TEXT("[EnemyAI] %s spawned at %s"), *GetName(), *GetActorLocation().ToString());
}

void AEnemyShip::HandleDeath(AActor* DeadActor)
{
	const FVector Loc = GetActorLocation();
	// Emissive expanding flash where the ship dies (M13 FX), then despawn.
	AExplosionFx::Spawn(GetWorld(), Loc, 900.f, FxMaterial, 0.65f);
	UE_LOG(LogTemp, Log, TEXT("[EnemyAI] %s destroyed -> despawning"), *GetName());
	Destroy();
}

AActor* AEnemyShip::GetPlayerTarget() const
{
	return UGameplayStatics::GetPlayerPawn(this, 0);
}

void AEnemyShip::SetAIState(EEnemyAIState NewState)
{
	if (AIState == NewState)
	{
		return;
	}
	AIState = NewState;

	const TCHAR* Name =
		NewState == EEnemyAIState::Engage   ? TEXT("Engage") :
		NewState == EEnemyAIState::Approach ? TEXT("Approach") :
		                                      TEXT("Idle");
	UE_LOG(LogTemp, Log, TEXT("[EnemyAI] %s state -> %s"), *GetName(), Name);
}

void AEnemyShip::FireAtPlayer(const AActor* Target)
{
	if (!Target)
	{
		return;
	}
	const FVector Start = GetActorLocation();
	const FVector End = Target->GetActorLocation();
	// Hostile orange beam (M13 emissive-mesh FX).
	ABeamFx::Spawn(GetWorld(), Start, End, FxMaterial, 16.f, 0.18f);
	UE_LOG(LogTemp, Log, TEXT("[EnemyAI] %s FIRE at %s (range %.0f uu)"),
		*GetName(), *Target->GetName(), FVector::Dist(Start, End));

	// Land damage on the player (shield power mitigates it; 0 hull → defeat).
	if (UHealthComponent* Health = Target->FindComponentByClass<UHealthComponent>())
	{
		Health->ApplyDamage(EnemyBeamDamage);
	}
}

void AEnemyShip::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Dead enemies stop acting (despawn/explosion arrives at M10).
	if (HealthComp && !HealthComp->IsAlive())
	{
		return;
	}

	const AActor* Player = GetPlayerTarget();
	if (!Player)
	{
		SetAIState(EEnemyAIState::Idle);
		return;
	}

	const FVector ToPlayer = Player->GetActorLocation() - GetActorLocation();
	const float Distance = ToPlayer.Size();

	// Yaw toward the player (yaw-only; space combat stays on a plane for the slice).
	if (Distance > 1.f)
	{
		const FRotator Current = GetActorRotation();
		const FRotator Desired = ToPlayer.Rotation();
		const FRotator Stepped = FMath::RInterpConstantTo(Current, Desired, DeltaSeconds, TurnRateDeg);
		SetActorRotation(FRotator(0.f, Stepped.Yaw, 0.f));
	}

	// Approach until inside the standoff bubble, then hold.
	const bool bInEngageRange = Distance <= EngageRange;
	if (Distance > StandoffDistance)
	{
		SetAIState(bInEngageRange ? EEnemyAIState::Engage : EEnemyAIState::Approach);
		AddActorWorldOffset(GetActorForwardVector() * MoveSpeed * DeltaSeconds, true);
	}
	else
	{
		SetAIState(EEnemyAIState::Engage);
	}

	// Fire on a fixed interval while in engage range.
	if (bInEngageRange)
	{
		FireCooldown -= DeltaSeconds;
		if (FireCooldown <= 0.f)
		{
			FireAtPlayer(Player);
			FireCooldown = FireInterval;
		}
	}
	else
	{
		FireCooldown = FireInterval;
	}
}
