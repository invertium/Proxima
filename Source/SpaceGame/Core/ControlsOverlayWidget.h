// SpaceGame — bridge simulator. In-game controls reference overlay (R2).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ControlsOverlayWidget.generated.h"

/**
 * UControlsOverlayWidget — the H-key keyboard reference card. Built in C++ via the WidgetTree
 * (no WBP), same rows as the main menu's CONTROLS panel (MenuUI::AddControlRows). Purely
 * informational: it does not pause the game or steal input — H toggles it off again.
 */
UCLASS()
class SPACEGAME_API UControlsOverlayWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void BuildUI();
};
