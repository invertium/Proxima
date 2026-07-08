// SpaceGame — bridge simulator. Settings overlay (R2).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SettingsMenuWidget.generated.h"

class UTextBlock;

/**
 * USettingsMenuWidget — the SETTINGS overlay reachable from both the main menu and the pause
 * menu. Built in C++ via the WidgetTree (no WBP). Each row is a single cycle-button showing
 * its current value: window mode, resolution, graphics quality, and master volume. Every
 * click applies immediately and persists (window/resolution/quality via UGameUserSettings →
 * GameUserSettings.ini; volume via FApp::SetVolumeMultiplier + our own config key, re-applied
 * at boot by USpaceGameInstance::Init).
 */
UCLASS()
class SPACEGAME_API USettingsMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Read the persisted master volume (0..1, default 1) from GameUserSettings.ini. */
	static float LoadMasterVolume();

	/** Apply the persisted master volume to the app (called from USpaceGameInstance::Init). */
	static void ApplyPersistedAudio();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void BuildUI();

	UFUNCTION() void OnCycleWindowMode();
	UFUNCTION() void OnCycleResolution();
	UFUNCTION() void OnCycleQuality();
	UFUNCTION() void OnCycleVolume();
	UFUNCTION() void OnBack();

	/** Refresh every row label from the live settings. */
	void UpdateLabels();

	/** Resolutions offered by the RESOLUTION row (display-supported, fallback list if none). */
	TArray<FIntPoint> Resolutions;

	UPROPERTY() TObjectPtr<UTextBlock> WindowModeLabel;
	UPROPERTY() TObjectPtr<UTextBlock> ResolutionLabel;
	UPROPERTY() TObjectPtr<UTextBlock> QualityLabel;
	UPROPERTY() TObjectPtr<UTextBlock> VolumeLabel;
};
