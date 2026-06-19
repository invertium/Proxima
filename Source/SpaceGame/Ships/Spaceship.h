// SpaceGame — bridge simulator. Ship pawn (simulation core, C++ per DECISIONS D5).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Spaceship.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class UShipMovementComponent;

/**
 * ASpaceship — the player's ship. M2: visual hull + 3rd-person follow camera.
 * Movement / subsystems (power, weapons, health) are added in later milestones
 * as UActorComponents so state stays replication-ready (DECISIONS D5/D7).
 */
UCLASS()
class SPACEGAME_API ASpaceship : public APawn
{
	GENERATED_BODY()

public:
	ASpaceship();

protected:
	virtual void BeginPlay() override;

	/** Unrotated root so camera/forward stay aligned with actor +X regardless of mesh orientation. */
	UPROPERTY(VisibleAnywhere, Category = "Ship")
	TObjectPtr<USceneComponent> ShipRoot;

	/** Placeholder hull mesh (primitive-first, DECISIONS D9). */
	UPROPERTY(VisibleAnywhere, Category = "Ship")
	TObjectPtr<UStaticMeshComponent> ShipMesh;

	UPROPERTY(VisibleAnywhere, Category = "Ship|Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, Category = "Ship|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	/** Impulse movement sim (throttle/turn). Public so test harness / Helm can drive it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship")
	TObjectPtr<UShipMovementComponent> MovementComp;
};
