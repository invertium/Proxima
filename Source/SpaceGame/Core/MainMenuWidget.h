// SpaceGame — bridge simulator. Host-machine main menu (M18).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

class UButton;
class UTextBlock;

/**
 * UMainMenuWidget — the startup front-end shown on the host "viewscreen". Built entirely in
 * C++ via the WidgetTree (no WBP asset), so it compiles + runs headlessly. Offers NEW GAME
 * (fresh campaign → first mission), CONTINUE (load the save → resume mission; disabled when
 * no save exists), and QUIT. The encounter map is opened via UGameplayStatics::OpenLevel.
 */
UCLASS()
class SPACEGAME_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	void BuildUI();

	UFUNCTION() void OnNewGame();   // -> ship select
	UFUNCTION() void OnContinue();
	UFUNCTION() void OnControls();  // -> keyboard-reference panel (R2)
	UFUNCTION() void OnQuit();

	// Ship select (shown after New Game).
	UFUNCTION() void OnPickInterceptor();
	UFUNCTION() void OnPickCruiser();
	UFUNCTION() void OnBack();

	/** Start a fresh campaign with the chosen ship. */
	void StartCampaign();

	UPROPERTY() TObjectPtr<class UBorder> Root;
	UPROPERTY() TObjectPtr<class UWidget> MainPanel;
	UPROPERTY() TObjectPtr<UButton> ContinueButton;
	UPROPERTY() TObjectPtr<UTextBlock> ContinueLabel;
};
