// SpaceGame — bridge simulator. End-of-encounter overlay (defeat M11 / victory M12).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EndScreenWidget.generated.h"

class UTextBlock;

/**
 * UEndScreenWidget — a full-screen outcome overlay shown when the encounter ends.
 * The controller fills it via SetOutcome() (big title + subtitle, tinted), so one
 * WBP serves both the M11 defeat and the M12 victory screens. Pure presentation —
 * the controller owns the pause/input handoff that brings it up.
 */
UCLASS()
class SPACEGAME_API UEndScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Set the headline + subtitle text and the title colour (e.g. red defeat / green victory). */
	UFUNCTION(BlueprintCallable, Category = "Bridge|EndScreen")
	void SetOutcome(const FText& Title, const FText& Subtitle, FLinearColor TitleColor);

protected:
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SubtitleText;
};
