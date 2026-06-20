// SpaceGame — bridge simulator. Player controller: station switching + HUD owner.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Core/StationTypes.h"
#include "Components/PowerComponent.h"
#include "BridgePlayerController.generated.h"

class UBridgeHUDWidget;

/**
 * ABridgePlayerController — owns the bridge HUD and the active-station state, and
 * switches stations on number keys 1/2/3 (DECISIONS D10). Per-station consoles
 * (Helm/Weapons/Engineering) are added to the HUD at M5–M8; this is the shell.
 */
UCLASS()
class SPACEGAME_API ABridgePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** Switch the active station, update the HUD, and log the change. */
	UFUNCTION(BlueprintCallable, Category = "Bridge")
	void SetStation(EStation NewStation);

	UFUNCTION(BlueprintPure, Category = "Bridge")
	EStation GetStation() const { return CurrentStation; }

	/** The Engineering system the player is currently editing (for console highlight). */
	UFUNCTION(BlueprintPure, Category = "Bridge")
	EShipSystem GetSelectedSystem() const { return SelectedSystem; }

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	/** Widget class for the HUD shell (set to WBP_BridgeHUD in the GameMode/BP defaults). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bridge")
	TSubclassOf<UBridgeHUDWidget> HUDWidgetClass;

private:
	void SelectHelm()        { SetStation(EStation::Helm); }
	void SelectWeapons()     { SetStation(EStation::Weapons); }
	void SelectEngineering() { SetStation(EStation::Engineering); }

	// --- Helm input (active only while CurrentStation == Helm, DECISIONS D10) ---
	void ThrottleUp();
	void ThrottleDown();
	void TurnLeft();
	void TurnRight();
	void TurnStop();

	/** Resolve the possessed ship's movement component, or null. */
	class UShipMovementComponent* GetShipMovement() const;

	/** Push the current throttle level onto the ship (clamped 0..1). */
	void ApplyThrottle();

	// --- Engineering input (active only while CurrentStation == Engineering) ---
	void EngSelectPrev();
	void EngSelectNext();
	void EngPowerUp();
	void EngPowerDown();

	/** Resolve the possessed ship's power component, or null. */
	class UPowerComponent* GetShipPower() const;

	UPROPERTY()
	TObjectPtr<UBridgeHUDWidget> HUDWidget;

	EStation CurrentStation = EStation::Helm;

	/** Helm throttle level, stepped by W/S; persists when leaving Helm so the ship coasts. */
	float ThrottleLevel = 0.f;

	/** Per-press throttle step. */
	static constexpr float ThrottleStep = 0.2f;

	/** Engineering system currently being edited (Up/Down adjust it, Left/Right select). */
	EShipSystem SelectedSystem = EShipSystem::Engine;

	/** Per-press power step. */
	static constexpr float PowerStep = 0.1f;
};
