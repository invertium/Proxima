// SpaceGame — bridge simulator. Helm console: throttle/speed/heading readouts + radar.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HelmConsoleWidget.generated.h"

class UTextBlock;
class UProgressBar;

/**
 * UHelmConsoleWidget — the Helm station console (DECISIONS D10). Polls the possessed
 * ship's UShipMovementComponent every frame and shows live throttle / speed / heading
 * readouts plus a throttle bar; the WBP child also embeds the tactical radar. The
 * console is driven by the player controller's W/S (throttle) + A/D (turn) binds; this
 * widget is read-only telemetry. Shown only while the Helm station is active (the HUD
 * toggles its visibility). Power will cap MaxSpeed/turn at M7.
 */
UCLASS()
class SPACEGAME_API UHelmConsoleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	// Bound from the WBP by name; optional so a partial layout still compiles/runs.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ThrottleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SpeedText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HeadingText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ThrottleBar;
};
