// SpaceGame — bridge simulator. Player controller: station switching + HUD owner.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Core/StationTypes.h"
#include "Components/PowerComponent.h"
#include "BridgePlayerController.generated.h"

class UBridgeHUDWidget;
class UEndScreenWidget;
class UPauseMenuWidget;
class UOutcomeMenuWidget;

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

	/** Select a system and step its power (mouse/touch path for the Engineering +/- buttons). */
	UFUNCTION(BlueprintCallable, Category = "Bridge")
	void EngAdjustSystem(EShipSystem System, bool bIncrease);

	/** (Re)subscribe to every current hostile's death — the win condition. Public so the mission
	 *  spawner can call it after it spawns the encounter (subsystems begin play after the PC). */
	UFUNCTION(BlueprintCallable, Category = "Bridge")
	void BindEnemyDeaths();

	/** Called by the open-sector director (UMissionSubsystem) when the final system is cleared:
	 *  raises the campaign epilogue outcome screen. Per-mission clears are seamless (no overlay). */
	void OnCampaignComplete();

	// --- Pause menu actions (called by UPauseMenuWidget's buttons, M18) ---
	void PauseResume();
	void PauseSave();
	void PauseRestart();
	void PauseMainMenu();
	void PauseQuit();

	// --- Outcome actions (called by UOutcomeMenuWidget's buttons, M18 campaign flow) ---
	void OutcomePrimary();
	void OutcomeSecondary();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;

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

	// --- Weapons input (active only while CurrentStation == Weapons) ---
	void WeaponCycleTarget();
	void WeaponFire();

	/** Resolve the possessed ship's weapon component, or null. */
	class UWeaponComponent* GetShipWeapon() const;

	// --- End-of-encounter (M11 defeat / M12 victory) ---
	/** Bound to the player ship's health OnDeath: flip the phase to Defeat (hostiles stand down)
	 *  and start the short beat before the overlay, so the crew sees the ship go up (M24). */
	UFUNCTION()
	void HandlePlayerDeath(AActor* DeadActor);

	/** Raise the defeat overlay after the death beat has played out. */
	void ShowDefeatOutcome();

	/** Bound to every hostile's health OnDeath: the last kill brings up victory. */
	UFUNCTION()
	void HandleEnemyDeath(AActor* DeadActor);

	/** Build + show the actionable outcome overlay (pauses + UI input). PrimaryKind selects
	 *  what the primary button does (next mission / retry / main menu). */
	void ShowOutcome(const FText& Title, FLinearColor Color, const FText& Subtitle,
		const FString& PrimaryLabel, const FString& SecondaryLabel);

	// --- Pause overlay (ESC, M18) ---
	/** Toggle the pause overlay: pause + show UI, or hide + resume. BlueprintCallable so it can
	 *  be driven for tests (PIE eats a real ESC press to stop play). */
	UFUNCTION(BlueprintCallable, Category = "Bridge")
	void TogglePause();
	void ShowPauseMenu();
	void HidePauseMenu();

	/** What the outcome overlay's primary button does. */
	enum class EOutcomeKind : uint8 { VictoryNext, VictoryComplete, Defeat };

	UPROPERTY()
	TObjectPtr<UBridgeHUDWidget> HUDWidget;

	UPROPERTY()
	TObjectPtr<UOutcomeMenuWidget> Outcome;

	EOutcomeKind OutcomeKind = EOutcomeKind::Defeat;

	/** Salvage banked this encounter (per-kill), shown on the victory screen (M19). */
	int32 EarnedCredits = 0;
	int32 EarnedXP = 0;

	UPROPERTY()
	TObjectPtr<UPauseMenuWidget> PauseMenu;

	/** True while the pause overlay is up (guards the toggle). */
	bool bPaused = false;

	/** Delays the defeat overlay after the player ship dies (M24 death beat). */
	FTimerHandle DefeatBeatTimer;

	/** Seconds between the player ship's death FX and the defeat overlay. */
	static constexpr float DefeatBeatSeconds = 2.2f;

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
