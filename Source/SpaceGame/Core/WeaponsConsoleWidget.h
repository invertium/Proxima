// SpaceGame — bridge simulator. Weapons console: target info + beam charge.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WeaponsConsoleWidget.generated.h"

class UTextBlock;
class UProgressBar;

/**
 * UWeaponsConsoleWidget — the Weapons station console (DECISIONS D10/D11). Polls the
 * possessed ship's UWeaponComponent each frame: shows the current target's name +
 * range + in/out-of-range status, and a beam charge bar (green when ready, amber while
 * charging). The controller cycles targets + fires (gated to the Weapons station); this
 * widget is read-only telemetry. The radar is embedded so the operator can see contacts.
 * Shown only while the Weapons station is active.
 */
UCLASS()
class SPACEGAME_API UWeaponsConsoleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TargetNameText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TargetRangeText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TargetStatusText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ChargeText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> ChargeBar;

	/** Charge bar colour when the beam is ready to fire. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapons")
	FLinearColor ReadyColor = FLinearColor(0.2f, 1.0f, 0.4f, 1.0f); // green

	/** Charge bar colour while still charging. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapons")
	FLinearColor ChargingColor = FLinearColor(1.0f, 0.7f, 0.15f, 1.0f); // amber
};
