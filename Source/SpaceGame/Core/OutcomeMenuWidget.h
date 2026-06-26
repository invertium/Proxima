// SpaceGame — bridge simulator. End-of-mission outcome overlay (M18 campaign flow).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OutcomeMenuWidget.generated.h"

class UButton;
class UTextBlock;

/**
 * UOutcomeMenuWidget — the actionable victory/defeat panel (C++ WidgetTree). Configured by the
 * controller via Setup() with a title, subtitle and up to two button labels, it drives the
 * campaign flow: NEXT MISSION / RETRY / MAIN MENU. Replaces the static WBP end screen so the
 * player can actually progress between missions. Buttons call back into ABridgePlayerController.
 */
UCLASS()
class SPACEGAME_API UOutcomeMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Configure before AddToViewport. An empty SecondaryLabel hides the secondary button. */
	void Setup(const FText& Title, FLinearColor TitleColor, const FText& Subtitle,
		const FString& PrimaryLabel, const FString& SecondaryLabel);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void BuildUI();

	UFUNCTION() void OnPrimary();
	UFUNCTION() void OnSecondary();

	class ABridgePlayerController* Owner() const;

	FText CfgTitle;
	FText CfgSubtitle;
	FLinearColor CfgColor = FLinearColor::White;
	FString CfgPrimary;
	FString CfgSecondary;
};
