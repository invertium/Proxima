// SpaceGame — bridge simulator. Enemy ship pawn + simple AI implementation.

#include "Ships/EnemyShip.h"

#include "Components/HealthComponent.h"
#include "Components/RadarContactComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "FX/BeamFx.h"
#include "FX/ExplosionFx.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

AEnemyShip::AEnemyShip()
{
	PrimaryActorTick.bCanEverTick = true;

	// No AIController: the AI lives in Tick (simple is fine for the vertical slice).
	AutoPossessAI = EAutoPossessAI::Disabled;

	ShipRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ShipRoot"));
	SetRootComponent(ShipRoot);

	// Imported enemy hull: Quaternius "Imperial" cruiser (CC0), red palette (M13/assets).
	static ConstructorHelpers::FObjectFinder<UStaticMesh> HullMesh(TEXT("/Game/Art/Meshes/Imperial.Imperial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> HullMat(TEXT("/Game/Art/Materials/M_Imperial.M_Imperial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> OrangeGlow(TEXT("/Game/Art/Materials/M_GlowOrange.M_GlowOrange"));

	// Main hull. Model length runs along local Y; yaw -90 swings the bow to actor +X.
	ShipMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShipMesh"));
	ShipMesh->SetupAttachment(ShipRoot);
	ShipMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ShipMesh->SetCollisionObjectType(ECC_Pawn);
	ShipMesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	ShipMesh->SetRelativeScale3D(FVector(0.6f));
	if (HullMesh.Succeeded()) { ShipMesh->SetStaticMesh(HullMesh.Object); }
	if (HullMat.Succeeded())  { ShipMesh->SetMaterial(0, HullMat.Object); }

	// Beam + explosion FX tint (M13).
	if (OrangeGlow.Succeeded()) { FxMaterial = OrangeGlow.Object; }

	// Enemy beam-fire SFX (CC0).
	static ConstructorHelpers::FObjectFinder<USoundBase> Fire(TEXT("/Game/Audio/S_EnemyFire.S_EnemyFire"));
	if (Fire.Succeeded()) { FireSound = Fire.Object; }

	// Radar blip (hostile red default) + makes this a valid player weapon target.
	RadarContact = CreateDefaultSubobject<URadarContactComponent>(TEXT("RadarContact"));

	// Hull + shields; the player's beam drains this at M10.
	HealthComp = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComp"));
}

void AEnemyShip::BeginPlay()
{
	Super::BeginPlay();

	// Reskin/retune for the archetype (set by the mission spawner before FinishSpawning).
	ApplyTypePreset();

	FireCooldown = FireInterval;

	// Despawn + explode when the player's beam drains our hull to zero (M10).
	if (HealthComp)
	{
		HealthComp->OnDeath.AddDynamic(this, &AEnemyShip::HandleDeath);
	}

	UE_LOG(LogTemp, Log, TEXT("[EnemyAI] %s (%s) spawned at %s"), *GetName(),
		ShipType == EEnemyType::Scout ? TEXT("Scout") : ShipType == EEnemyType::Cruiser ? TEXT("Cruiser") : TEXT("Gunship"),
		*GetActorLocation().ToString());
}

void AEnemyShip::ApplyTypePreset()
{
	// Per-archetype mesh + material + scale + AI/health tuning. Reuses the two existing meshes
	// (Imperial cruiser, Insurgent fighter) recoloured/rescaled — no new art needed.
	auto Load = [](const TCHAR* Path) -> UObject*
	{
		return StaticLoadObject(UObject::StaticClass(), nullptr, Path);
	};

	const TCHAR* MeshPath = TEXT("/Game/Art/Meshes/Imperial.Imperial");
	const TCHAR* MatPath  = TEXT("/Game/Art/Materials/M_Imperial.M_Imperial");
	const TCHAR* FxPath   = TEXT("/Game/Art/Materials/M_GlowOrange.M_GlowOrange");
	float Scale = 0.6f;
	FLinearColor Blip(1.0f, 0.25f, 0.2f, 1.f);

	switch (ShipType)
	{
	case EEnemyType::Scout:
		MeshPath = TEXT("/Game/Art/Meshes/Insurgent.Insurgent");
		MatPath  = TEXT("/Game/Art/Materials/M_Insurgent.M_Insurgent");
		FxPath   = TEXT("/Game/Art/Materials/M_GlowCyan.M_GlowCyan");
		Scale = 0.4f;
		Blip = FLinearColor(0.3f, 0.9f, 1.0f, 1.f);
		MoveSpeed = 1900.f; TurnRateDeg = 80.f; StandoffDistance = 4500.f;
		EngageRange = 10000.f; FireInterval = 1.6f; EnemyBeamDamage = 5.f;
		if (HealthComp) { HealthComp->MaxHull = 50.f;  HealthComp->MaxShield = 20.f; }
		break;

	case EEnemyType::Cruiser:
		FxPath = TEXT("/Game/Art/Materials/M_GlowRed.M_GlowRed");
		Scale = 1.05f;
		Blip = FLinearColor(1.0f, 0.15f, 0.12f, 1.f);
		MoveSpeed = 700.f; TurnRateDeg = 32.f; StandoffDistance = 7500.f;
		EngageRange = 14000.f; FireInterval = 3.2f; EnemyBeamDamage = 14.f;
		if (HealthComp) { HealthComp->MaxHull = 220.f; HealthComp->MaxShield = 110.f; }
		break;

	case EEnemyType::Gunship:
	default:
		MoveSpeed = 1100.f; TurnRateDeg = 50.f; StandoffDistance = 6000.f;
		EngageRange = 12000.f; FireInterval = 2.5f; EnemyBeamDamage = 8.f;
		if (HealthComp) { HealthComp->MaxHull = 100.f; HealthComp->MaxShield = 50.f; }
		break;
	}

	if (ShipMesh)
	{
		if (UStaticMesh* M = Cast<UStaticMesh>(Load(MeshPath)))   { ShipMesh->SetStaticMesh(M); }
		if (UMaterialInterface* Mat = Cast<UMaterialInterface>(Load(MatPath))) { ShipMesh->SetMaterial(0, Mat); }
		ShipMesh->SetRelativeScale3D(FVector(Scale));
	}
	if (UMaterialInterface* Fx = Cast<UMaterialInterface>(Load(FxPath))) { FxMaterial = Fx; }
	if (RadarContact) { RadarContact->BlipColor = Blip; }

	// New Max values just landed — refill the pools so the ship starts at full for its type.
	if (HealthComp) { HealthComp->ResetPools(); }
}

void AEnemyShip::HandleDeath(AActor* DeadActor)
{
	UWorld* World = GetWorld();
	const FVector Loc = GetActorLocation();

	// Main blast (with the boom SFX), then a scatter of smaller silent flashes around it
	// for a richer multi-burst death (M14). These are independent actors, so they outlive
	// this ship being destroyed below.
	AExplosionFx::Spawn(World, Loc, 900.f, FxMaterial, 0.65f);
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector Offset = FMath::VRand() * FMath::FRandRange(200.f, 650.f);
		AExplosionFx::Spawn(World, Loc + Offset, FMath::FRandRange(240.f, 460.f),
			FxMaterial, FMath::FRandRange(0.3f, 0.5f), false);
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
	// Hostile orange beam (M13 emissive-mesh FX) + spatialized fire SFX (M14).
	ABeamFx::Spawn(GetWorld(), Start, End, FxMaterial, 16.f, 0.18f);
	if (FireSound)
	{
		// Per-shot pitch jitter so repeated enemy fire stays lively (M14).
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, Start, 1.f, FMath::FRandRange(0.94f, 1.06f));
	}
	UE_LOG(LogTemp, Log, TEXT("[EnemyAI] %s FIRE at %s (range %.0f uu)"),
		*GetName(), *Target->GetName(), FVector::Dist(Start, End));

	// Land damage on the player (shield power mitigates it; 0 hull → defeat).
	if (UHealthComponent* Health = Target->FindComponentByClass<UHealthComponent>())
	{
		Health->ApplyDamage(EnemyBeamDamage);
	}

	// Small orange impact flash on the player (no boom — bPlaySound off).
	AExplosionFx::Spawn(GetWorld(), End, 220.f, FxMaterial, 0.2f, false);
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
