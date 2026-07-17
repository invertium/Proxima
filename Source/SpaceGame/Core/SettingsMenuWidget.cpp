// SpaceGame — bridge simulator. Settings overlay implementation.

#include "Core/SettingsMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/MenuUI.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"

using MenuUI::MakeText;
using MenuUI::AddFlatButton;

namespace
{
	const TCHAR* AudioConfigSection = TEXT("SpaceGame.Audio");
	const TCHAR* AudioConfigKey = TEXT("MasterVolume");
	const TCHAR* VideoConfigSection = TEXT("SpaceGame.Video");
	const TCHAR* VideoSeededKey = TEXT("DefaultsSeeded");
	const TCHAR* OnlineConfigSection = TEXT("SpaceGame.Online");
	const TCHAR* OnlineJoinKey = TEXT("EnableJoinCode");

	/** Volume moves in 10% steps; a click cycles down and wraps 0 → 100. */
	constexpr float VolumeStep = 0.1f;

	UGameUserSettings* Settings() { return UGameUserSettings::GetGameUserSettings(); }

	FString WindowModeName(EWindowMode::Type Mode)
	{
		switch (Mode)
		{
		case EWindowMode::Fullscreen:         return TEXT("FULLSCREEN");
		case EWindowMode::WindowedFullscreen: return TEXT("BORDERLESS");
		default:                              return TEXT("WINDOWED");
		}
	}

	FString QualityName(int32 Level)
	{
		switch (Level)
		{
		case 0:  return TEXT("LOW");
		case 1:  return TEXT("MEDIUM");
		case 2:  return TEXT("HIGH");
		case 3:  return TEXT("EPIC");
		default: return TEXT("CUSTOM");
		}
	}

	/** MAX FRAMERATE cycle options. 0 = uncapped. On a 60 Hz display an uncapped ~600 fps
	 *  render floods the compositor and hitches under fullscreen vsync — capping to the refresh
	 *  gives a steady present cadence, so 60 leads the list. */
	const TArray<float> FrameLimitOptions = { 60.f, 120.f, 144.f, 240.f, 0.f, 30.f };

	FString FrameLimitName(float Limit)
	{
		return Limit <= 0.f ? TEXT("UNLIMITED") : FString::Printf(TEXT("%d FPS"), FMath::RoundToInt(Limit));
	}

	/** The *effective* online-join state = the live sg.OnlineJoinCode CVar (what RequestJoinCode()
	 *  actually reads), falling back to the persisted config only if the CVar isn't registered. The UI
	 *  reads this — not the raw config — so the row can never disagree with what the game will send even
	 *  when the CVar was set from the console (a higher priority than our game-setting write). */
	bool OnlineJoinEffective(bool bConfigFallback)
	{
		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("sg.OnlineJoinCode")))
		{
			return CVar->GetInt() != 0;
		}
		return bConfigFallback;
	}
}

float USettingsMenuWidget::LoadMasterVolume()
{
	float Volume = 1.f;
	if (GConfig)
	{
		GConfig->GetFloat(AudioConfigSection, AudioConfigKey, Volume, GGameUserSettingsIni);
	}
	return FMath::Clamp(Volume, 0.f, 1.f);
}

void USettingsMenuWidget::ApplyPersistedAudio()
{
	FApp::SetVolumeMultiplier(LoadMasterVolume());
}

bool USettingsMenuWidget::LoadOnlineJoinEnabled()
{
	bool bEnabled = false; // opt-in: off unless the player has turned it on
	if (GConfig)
	{
		GConfig->GetBool(OnlineConfigSection, OnlineJoinKey, bEnabled, GGameUserSettingsIni);
	}
	return bEnabled;
}

void USettingsMenuWidget::ApplyPersistedOnlineJoin()
{
	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("sg.OnlineJoinCode")))
	{
		CVar->Set(LoadOnlineJoinEnabled() ? 1 : 0, ECVF_SetByGameSetting);
	}
}

void USettingsMenuWidget::SeedVideoDefaults()
{
	if (!GConfig) { return; }
	bool bSeeded = false;
	GConfig->GetBool(VideoConfigSection, VideoSeededKey, bSeeded, GGameUserSettingsIni);
	if (bSeeded) { return; } // player has launched before — respect whatever they've set since

	if (UGameUserSettings* S = Settings())
	{
		S->SetVSyncEnabled(true);
		S->SetFrameRateLimit(60.f);
		S->ApplyNonResolutionSettings(); // pushes r.VSync + t.MaxFPS now, without touching resolution
		S->SaveSettings();
	}
	GConfig->SetBool(VideoConfigSection, VideoSeededKey, true, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

TSharedRef<SWidget> USettingsMenuWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildUI();
	}
	return Super::RebuildWidget();
}

void USettingsMenuWidget::BuildUI()
{
	// Offer the display's own modes; fall back to common 16:9 sizes if the query is empty
	// (headless/PIE, some Wayland setups).
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
	if (Resolutions.IsEmpty())
	{
		Resolutions = { {1280, 720}, {1600, 900}, {1920, 1080}, {2560, 1440}, {3440, 1440}, {3840, 2160} };
	}

	UBorder* Root = WidgetTree->ConstructWidget<UBorder>();
	Root->SetBrushColor(FLinearColor(0.01f, 0.02f, 0.04f, 0.92f));
	Root->SetHorizontalAlignment(HAlign_Center);
	Root->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Root;

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	Root->SetContent(Box);

	if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(
		MakeText(WidgetTree, FText::FromString(TEXT("SETTINGS")), 36, FLinearColor(0.6f, 0.85f, 1.f, 1.f))))
	{
		S->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		S->SetHorizontalAlignment(HAlign_Center);
	}
	if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(
		MakeText(WidgetTree, FText::FromString(TEXT("click a row to cycle — applied and saved immediately")), 13,
			FLinearColor(0.5f, 0.6f, 0.7f, 1.f))))
	{
		S->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));
		S->SetHorizontalAlignment(HAlign_Center);
	}

	UButton* WindowBtn = AddFlatButton(WidgetTree, Box, TEXT(""));
	WindowModeLabel = Cast<UTextBlock>(WindowBtn->GetChildAt(0));
	WindowBtn->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnCycleWindowMode);

	UButton* ResBtn = AddFlatButton(WidgetTree, Box, TEXT(""));
	ResolutionLabel = Cast<UTextBlock>(ResBtn->GetChildAt(0));
	ResBtn->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnCycleResolution);

	UButton* QualityBtn = AddFlatButton(WidgetTree, Box, TEXT(""));
	QualityLabel = Cast<UTextBlock>(QualityBtn->GetChildAt(0));
	QualityBtn->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnCycleQuality);

	UButton* VSyncBtn = AddFlatButton(WidgetTree, Box, TEXT(""));
	VSyncLabel = Cast<UTextBlock>(VSyncBtn->GetChildAt(0));
	VSyncBtn->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnToggleVSync);

	UButton* FrameLimitBtn = AddFlatButton(WidgetTree, Box, TEXT(""));
	FrameLimitLabel = Cast<UTextBlock>(FrameLimitBtn->GetChildAt(0));
	FrameLimitBtn->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnCycleFrameLimit);

	UButton* VolumeBtn = AddFlatButton(WidgetTree, Box, TEXT(""));
	VolumeLabel = Cast<UTextBlock>(VolumeBtn->GetChildAt(0));
	VolumeBtn->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnCycleVolume);

	UButton* OnlineJoinBtn = AddFlatButton(WidgetTree, Box, TEXT(""));
	OnlineJoinLabel = Cast<UTextBlock>(OnlineJoinBtn->GetChildAt(0));
	OnlineJoinBtn->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnToggleOnlineJoin);

	UButton* BackBtn = AddFlatButton(WidgetTree, Box, TEXT("BACK"));
	BackBtn->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnBack);
	if (UVerticalBoxSlot* BS = Cast<UVerticalBoxSlot>(BackBtn->Slot))
	{
		BS->SetPadding(FMargin(0.f, 24.f, 0.f, 6.f));
	}

	UpdateLabels();
}

void USettingsMenuWidget::UpdateLabels()
{
	const UGameUserSettings* S = Settings();
	if (!S) { return; }

	if (WindowModeLabel)
	{
		WindowModeLabel->SetText(FText::FromString(
			FString::Printf(TEXT("WINDOW MODE   —   %s"), *WindowModeName(S->GetFullscreenMode()))));
	}
	if (ResolutionLabel)
	{
		const FIntPoint R = S->GetScreenResolution();
		ResolutionLabel->SetText(FText::FromString(
			FString::Printf(TEXT("RESOLUTION   —   %d x %d"), R.X, R.Y)));
	}
	if (QualityLabel)
	{
		QualityLabel->SetText(FText::FromString(
			FString::Printf(TEXT("QUALITY   —   %s"), *QualityName(S->GetOverallScalabilityLevel()))));
	}
	if (VSyncLabel)
	{
		VSyncLabel->SetText(FText::FromString(
			FString::Printf(TEXT("VSYNC   —   %s"), S->IsVSyncEnabled() ? TEXT("ON") : TEXT("OFF"))));
	}
	if (FrameLimitLabel)
	{
		FrameLimitLabel->SetText(FText::FromString(
			FString::Printf(TEXT("MAX FRAMERATE   —   %s"), *FrameLimitName(S->GetFrameRateLimit()))));
	}
	if (VolumeLabel)
	{
		VolumeLabel->SetText(FText::FromString(
			FString::Printf(TEXT("MASTER VOLUME   —   %d%%"), FMath::RoundToInt(LoadMasterVolume() * 100.f))));
	}
	if (OnlineJoinLabel)
	{
		const bool bOn = OnlineJoinEffective(LoadOnlineJoinEnabled());
		OnlineJoinLabel->SetText(FText::FromString(
			FString::Printf(TEXT("ONLINE JOIN CODE   —   %s"), bOn ? TEXT("ON") : TEXT("OFF"))));
	}
}

void USettingsMenuWidget::OnCycleWindowMode()
{
	UGameUserSettings* S = Settings();
	if (!S) { return; }
	const EWindowMode::Type Next = static_cast<EWindowMode::Type>(
		(static_cast<int32>(S->GetFullscreenMode()) + 1) % 3); // Fullscreen → Borderless → Windowed
	S->SetFullscreenMode(Next);
	S->ApplySettings(false);
	S->SaveSettings();
	UpdateLabels();
}

void USettingsMenuWidget::OnCycleResolution()
{
	UGameUserSettings* S = Settings();
	if (!S || Resolutions.IsEmpty()) { return; }
	const int32 Cur = Resolutions.IndexOfByKey(S->GetScreenResolution());
	S->SetScreenResolution(Resolutions[(Cur + 1) % Resolutions.Num()]);
	S->ApplySettings(false);
	S->SaveSettings();
	UpdateLabels();
}

void USettingsMenuWidget::OnCycleQuality()
{
	UGameUserSettings* S = Settings();
	if (!S) { return; }
	// From CUSTOM (-1) the first click lands on LOW; otherwise step LOW→MEDIUM→HIGH→EPIC→LOW.
	const int32 Next = (S->GetOverallScalabilityLevel() + 1) % 4;
	S->SetOverallScalabilityLevel(FMath::Max(0, Next));
	S->ApplySettings(false);
	S->SaveSettings();
	UpdateLabels();
}

void USettingsMenuWidget::OnToggleVSync()
{
	UGameUserSettings* S = Settings();
	if (!S) { return; }
	S->SetVSyncEnabled(!S->IsVSyncEnabled());
	S->ApplySettings(false); // pushes r.VSync
	S->SaveSettings();
	UpdateLabels();
}

void USettingsMenuWidget::OnCycleFrameLimit()
{
	UGameUserSettings* S = Settings();
	if (!S || FrameLimitOptions.IsEmpty()) { return; }
	// Find the current cap in the list (tolerant match), then step to the next.
	const float Cur = S->GetFrameRateLimit();
	int32 Idx = FrameLimitOptions.IndexOfByPredicate(
		[Cur](float V) { return FMath::IsNearlyEqual(V, Cur, 0.5f); });
	if (Idx == INDEX_NONE) { Idx = -1; } // an off-list value (e.g. baked 0) advances to the first option
	S->SetFrameRateLimit(FrameLimitOptions[(Idx + 1) % FrameLimitOptions.Num()]);
	S->ApplySettings(false); // pushes t.MaxFPS
	S->SaveSettings();
	UpdateLabels();
}

void USettingsMenuWidget::OnCycleVolume()
{
	// Step down 10% per click, wrapping 0% back up to 100%.
	float Volume = LoadMasterVolume() - VolumeStep;
	if (Volume < -KINDA_SMALL_NUMBER) { Volume = 1.f; }
	Volume = FMath::Clamp(Volume, 0.f, 1.f);

	FApp::SetVolumeMultiplier(Volume);
	if (GConfig)
	{
		GConfig->SetFloat(AudioConfigSection, AudioConfigKey, Volume, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	UpdateLabels();
}

void USettingsMenuWidget::OnToggleOnlineJoin()
{
	// Opt-in: flip the *effective* state, persist it, and push it into the CVar so the next hosted world
	// registers (or stops registering) its crew URL with proxima-join. Off by default — nothing is
	// sent to the online service unless the player turns this on here.
	const bool bNext = !OnlineJoinEffective(LoadOnlineJoinEnabled());
	if (GConfig)
	{
		GConfig->SetBool(OnlineConfigSection, OnlineJoinKey, bNext, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	// SetByConsole is the top CVar priority, so this explicit player action always wins — even over a
	// prior `sg.OnlineJoinCode` typed at the console. Without it a console-set value would silently
	// outrank a game-setting write and the row could claim OFF while the game still registered (P2).
	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("sg.OnlineJoinCode")))
	{
		CVar->Set(bNext ? 1 : 0, ECVF_SetByConsole);
	}
	UpdateLabels();
}

void USettingsMenuWidget::OnBack()
{
	RemoveFromParent();
}
