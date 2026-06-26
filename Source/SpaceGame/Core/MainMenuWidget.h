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

	UFUNCTION() void OnNewGame();
	UFUNCTION() void OnContinue();
	UFUNCTION() void OnQuit();

	UPROPERTY() TObjectPtr<UButton> ContinueButton;
	UPROPERTY() TObjectPtr<UTextBlock> ContinueLabel;
};
