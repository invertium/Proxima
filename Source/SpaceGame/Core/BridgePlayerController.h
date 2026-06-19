// SpaceGame — bridge simulator. Player controller: station switching + HUD owner.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Core/StationTypes.h"
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

	UPROPERTY()
	TObjectPtr<UBridgeHUDWidget> HUDWidget;

	EStation CurrentStation = EStation::Helm;
};
