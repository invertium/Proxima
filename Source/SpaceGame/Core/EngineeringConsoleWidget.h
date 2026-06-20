// SpaceGame — bridge simulator. Engineering console: power bars + reactor load.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EngineeringConsoleWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UButton;

/**
 * UEngineeringConsoleWidget — the Engineering station console (DECISIONS D10/D11).
 * Polls the possessed ship's UPowerComponent each frame and shows a power bar + %
 * readout per system (Engine / Weapons / Shields) plus the reactor-load total. The
 * row matching the player controller's currently-selected system is highlighted;
 * the controller's arrow keys select a row and raise/lower its power. Read-only
 * telemetry — the controller owns the mutation. Shown only at the Engineering station.
 */
UCLASS()
class SPACEGAME_API UEngineeringConsoleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	// Bound from the WBP by name (optional so a partial layout still runs).
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> EngineText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> WeaponsText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ShieldsText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ReactorText;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> EngineBar;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> WeaponsBar;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> ShieldsBar;

	// Per-system −/+ buttons (mouse/touch). Optional so the console still runs key-only.
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> EngineMinusBtn;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> EnginePlusBtn;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> WeaponsMinusBtn;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> WeaponsPlusBtn;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> ShieldsMinusBtn;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> ShieldsPlusBtn;

	// OnClicked handlers — each selects its row and steps that system's power.
	UFUNCTION() void OnEngineMinus();
	UFUNCTION() void OnEnginePlus();
	UFUNCTION() void OnWeaponsMinus();
	UFUNCTION() void OnWeaponsPlus();
	UFUNCTION() void OnShieldsMinus();
	UFUNCTION() void OnShieldsPlus();

	/** Highlight colour for the selected system's label. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engineering")
	FLinearColor SelectedColor = FLinearColor(1.0f, 0.85f, 0.2f, 1.0f); // amber

	/** Dimmed colour for non-selected systems. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engineering")
	FLinearColor IdleColor = FLinearColor(0.55f, 0.7f, 0.8f, 1.0f);
};
