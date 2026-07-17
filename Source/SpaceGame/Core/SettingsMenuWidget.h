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

	/** Read the persisted "online join code" opt-in (default false — the feature only phones the
	 *  proxima-join service when the player explicitly turns it on). */
	static bool LoadOnlineJoinEnabled();

	/** Push the persisted opt-in into the sg.OnlineJoinCode CVar so RequestJoinCode() honours the
	 *  player's saved choice from the first world (called from USpaceGameInstance::Init). */
	static void ApplyPersistedOnlineJoin();

	/** First-launch only: default to vsync on + a 60 FPS cap so the game presents smoothly on a
	 *  60 Hz display (uncapped ~600 fps hitches under fullscreen vsync). Guarded by a one-shot flag
	 *  so it never overrides a choice the player later makes via the VSYNC / MAX FRAMERATE rows.
	 *  Called from USpaceGameInstance::Init. */
	static void SeedVideoDefaults();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void BuildUI();

	UFUNCTION() void OnCycleWindowMode();
	UFUNCTION() void OnCycleResolution();
	UFUNCTION() void OnCycleQuality();
	UFUNCTION() void OnToggleVSync();
	UFUNCTION() void OnCycleFrameLimit();
	UFUNCTION() void OnCycleVolume();
	UFUNCTION() void OnToggleOnlineJoin();
	UFUNCTION() void OnBack();

	/** Refresh every row label from the live settings. */
	void UpdateLabels();

	/** Resolutions offered by the RESOLUTION row (display-supported, fallback list if none). */
	TArray<FIntPoint> Resolutions;

	UPROPERTY() TObjectPtr<UTextBlock> WindowModeLabel;
	UPROPERTY() TObjectPtr<UTextBlock> ResolutionLabel;
	UPROPERTY() TObjectPtr<UTextBlock> QualityLabel;
	UPROPERTY() TObjectPtr<UTextBlock> VSyncLabel;
	UPROPERTY() TObjectPtr<UTextBlock> FrameLimitLabel;
	UPROPERTY() TObjectPtr<UTextBlock> VolumeLabel;
	UPROPERTY() TObjectPtr<UTextBlock> OnlineJoinLabel;
};
