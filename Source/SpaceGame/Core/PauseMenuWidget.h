// SpaceGame — bridge simulator. In-game pause overlay (M18).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PauseMenuWidget.generated.h"

class UButton;
class UTextBlock;

/**
 * UPauseMenuWidget — the ESC pause overlay on the host viewscreen. Built in C++ via the
 * WidgetTree (no WBP). Buttons drive the owning ABridgePlayerController: RESUME / SAVE /
 * RESTART / MAIN MENU / QUIT. The controller owns the pause/unpause + input handoff; this
 * widget is just the panel. A transient SAVED toast confirms a successful save.
 */
UCLASS()
class SPACEGAME_API UPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Flash a short "CAMPAIGN SAVED" confirmation under the buttons. */
	void ShowSavedToast();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void BuildUI();

	UFUNCTION() void OnResume();
	UFUNCTION() void OnSave();
	UFUNCTION() void OnRestart();
	UFUNCTION() void OnSettings();
	UFUNCTION() void OnMainMenu();
	UFUNCTION() void OnQuit();

	// Leaving mid-mission drops progress since the last save, so both exits confirm first (R2).
	UFUNCTION() void OnConfirmProceed();
	UFUNCTION() void OnConfirmBack();

	/** Which exit the confirm panel's CONFIRM button executes. */
	enum class EPendingExit : uint8 { None, MainMenu, Quit };

	/** Swap the panel to a confirm prompt; CONFIRM runs the pending exit, BACK restores. */
	void ShowConfirm(const FString& Title, const FString& ConfirmLabel, EPendingExit Exit);

	class ABridgePlayerController* Owner() const;

	EPendingExit PendingExit = EPendingExit::None;

	UPROPERTY() TObjectPtr<class UBorder> Root;
	UPROPERTY() TObjectPtr<class UWidget> MainPanel;
	UPROPERTY() TObjectPtr<UTextBlock> Toast;
};
