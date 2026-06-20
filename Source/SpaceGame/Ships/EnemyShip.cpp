// SpaceGame — bridge simulator. Enemy ship pawn + simple AI implementation.

#include "Ships/EnemyShip.h"

#include "Components/HealthComponent.h"
#include "Components/RadarContactComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

AEnemyShip::AEnemyShip()
{
	PrimaryActorTick.bCanEverTick = true;

	// No AIController: the AI lives in Tick (simple is fine for the vertical slice).
	AutoPossessAI = EAutoPossessAI::Disabled;

	ShipRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ShipRoot"));
	SetRootComponent(ShipRoot);

	// Placeholder hull: a cube so it reads visually distinct from the player's cone.
	ShipMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShipMesh"));
	ShipMesh->SetupAttachment(ShipRoot);
	ShipMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ShipMesh->SetCollisionObjectType(ECC_Pawn);
	ShipMesh->SetRelativeScale3D(FVector(2.f, 2.f, 1.f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		ShipMesh->SetStaticMesh(CubeMesh.Object);
	}

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
	if (const UWorld* World = GetWorld())
	{
		// Primitive-first explosion: concentric debris shells persist ~2s so the kill
		// reads on screen even after the actor is gone (Niagara FX is polish, M13).
		DrawDebugSphere(World, Loc, 300.f, 16, FColor(255, 200, 60), false, 2.0f, 0, 9.f);
		DrawDebugSphere(World, Loc, 650.f, 16, FColor(255, 110, 25), false, 2.0f, 0, 6.f);
		DrawDebugSphere(World, Loc, 1000.f, 12, FColor(140, 45, 12), false, 2.0f, 0, 3.f);
	}
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
	if (const UWorld* World = GetWorld())
	{
		// Hostile orange beam; player damage lands at M11, here it's the AI tell.
		DrawDebugLine(World, Start, End, FColor(255, 140, 40), false, 0.4f, 0, 10.f);
	}
	UE_LOG(LogTemp, Log, TEXT("[EnemyAI] %s FIRE at %s (range %.0f uu)"),
		*GetName(), *Target->GetName(), FVector::Dist(Start, End));
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
