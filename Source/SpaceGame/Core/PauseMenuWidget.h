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
	UFUNCTION() void OnMainMenu();
	UFUNCTION() void OnQuit();

	class ABridgePlayerController* Owner() const;

	UPROPERTY() TObjectPtr<UTextBlock> Toast;
};
