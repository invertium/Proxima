// SpaceGame — bridge simulator. HUD shell widget (station bar + active console).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/StationTypes.h"
#include "BridgeHUDWidget.generated.h"

class UTextBlock;

/**
 * UBridgeHUDWidget — the bridge HUD shell. The WBP child (WBP_BridgeHUD) supplies
 * the visual layout; this C++ base owns the logic so we avoid MCP node-graph
 * authoring (DECISIONS D5). It exposes a station-select bar (three labels) plus a
 * big active-console label. SetActiveStation() highlights the active tab and names
 * the console. Real consoles (Helm/Weapons/Engineering UMG) slot in at M5–M8.
 */
UCLASS()
class SPACEGAME_API UBridgeHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Update the bar highlight + the big console label for the active station. */
	UFUNCTION(BlueprintCallable, Category = "Bridge|HUD")
	void SetActiveStation(EStation Station);

protected:
	virtual void NativeConstruct() override;

	// --- Bound from the WBP by matching widget name (meta = BindWidget) ---
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HelmTab;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> WeaponsTab;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> EngineeringTab;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ActiveConsoleLabel;

	/** Highlight colour for the active tab. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bridge|HUD")
	FLinearColor ActiveColor = FLinearColor(0.15f, 0.85f, 1.0f, 1.0f); // cyan

	/** Dimmed colour for inactive tabs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bridge|HUD")
	FLinearColor InactiveColor = FLinearColor(0.4f, 0.45f, 0.5f, 1.0f); // grey-blue

private:
	void ApplyTab(UTextBlock* Tab, bool bActive);
};
