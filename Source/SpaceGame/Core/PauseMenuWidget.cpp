// SpaceGame — bridge simulator. Pause overlay implementation.

#include "Core/PauseMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/BridgePlayerController.h"
#include "Core/SettingsMenuWidget.h"
#include "Net/StationServerSubsystem.h"
#include "Core/MenuUI.h"

using MenuUI::MakeText;
using MenuUI::AddFlatButton;

TSharedRef<SWidget> UPauseMenuWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildUI();
	}
	return Super::RebuildWidget();
}

void UPauseMenuWidget::BuildUI()
{
	Root = WidgetTree->ConstructWidget<UBorder>();
	Root->SetBrushColor(FLinearColor(0.01f, 0.02f, 0.04f, 0.85f)); // dim the paused scene
	Root->SetHorizontalAlignment(HAlign_Center);
	Root->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Root;

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	MainPanel = Box;
	Root->SetContent(Box);

	if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(
		MakeText(WidgetTree, FText::FromString(TEXT("PAUSED")), 40, FLinearColor(0.6f, 0.85f, 1.f, 1.f))))
	{
		S->SetPadding(FMargin(0.f, 0.f, 0.f, 24.f));
		S->SetHorizontalAlignment(HAlign_Center);
	}

	AddFlatButton(WidgetTree, Box, TEXT("RESUME"))->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnResume);
	AddFlatButton(WidgetTree, Box, TEXT("SAVE"))->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnSave);
	AddFlatButton(WidgetTree, Box, TEXT("RESTART"))->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnRestart);
	AddFlatButton(WidgetTree, Box, TEXT("SETTINGS"))->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnSettings);
	AddFlatButton(WidgetTree, Box, TEXT("MAIN MENU"))->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnMainMenu);
	AddFlatButton(WidgetTree, Box, TEXT("QUIT"))->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnQuit);

	// Crew join URL (R2): so latecomers can pull up a console mid-session without reading logs.
	const FString CrewUrl = UStationServerSubsystem::GetCrewUrl();
	if (!CrewUrl.IsEmpty())
	{
		UTextBlock* Crew = MakeText(WidgetTree,
			FText::FromString(FString::Printf(TEXT("CREW CONSOLES  →  %s"), *CrewUrl)),
			14, FLinearColor(0.35f, 0.8f, 0.75f, 1.f));
		if (UVerticalBoxSlot* CS = Box->AddChildToVerticalBox(Crew))
		{
			CS->SetPadding(FMargin(0.f, 18.f, 0.f, 0.f));
			CS->SetHorizontalAlignment(HAlign_Center);
		}
	}

	Toast = MakeText(WidgetTree, FText::GetEmpty(), 16, FLinearColor(0.4f, 1.f, 0.55f, 1.f));
	if (UVerticalBoxSlot* TS = Box->AddChildToVerticalBox(Toast))
	{
		TS->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f));
		TS->SetHorizontalAlignment(HAlign_Center);
	}
}

ABridgePlayerController* UPauseMenuWidget::Owner() const
{
	return Cast<ABridgePlayerController>(GetOwningPlayer());
}

void UPauseMenuWidget::ShowSavedToast()
{
	if (Toast) { Toast->SetText(FText::FromString(TEXT("CAMPAIGN SAVED"))); }
}

void UPauseMenuWidget::OnSettings()
{
	// Shared settings overlay, above the pause panel (viewport z 130 vs 120).
	if (USettingsMenuWidget* Panel = CreateWidget<USettingsMenuWidget>(GetOwningPlayer(), USettingsMenuWidget::StaticClass()))
	{
		Panel->AddToViewport(130);
	}
}

void UPauseMenuWidget::OnResume()   { if (ABridgePlayerController* C = Owner()) { C->PauseResume(); } }
void UPauseMenuWidget::OnSave()     { if (ABridgePlayerController* C = Owner()) { C->PauseSave(); } }
void UPauseMenuWidget::OnRestart()  { if (ABridgePlayerController* C = Owner()) { C->PauseRestart(); } }

// Both mid-mission exits confirm first — progress since the last save is lost (R2).
void UPauseMenuWidget::OnMainMenu()
{
	ShowConfirm(TEXT("RETURN TO MAIN MENU?"), TEXT("MAIN MENU"), EPendingExit::MainMenu);
}

void UPauseMenuWidget::OnQuit()
{
	ShowConfirm(TEXT("QUIT TO DESKTOP?"), TEXT("QUIT"), EPendingExit::Quit);
}

void UPauseMenuWidget::ShowConfirm(const FString& Title, const FString& ConfirmLabel, EPendingExit Exit)
{
	PendingExit = Exit;

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(
		MakeText(WidgetTree, FText::FromString(Title), 30, FLinearColor(1.f, 0.75f, 0.35f, 1.f))))
	{
		S->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
		S->SetHorizontalAlignment(HAlign_Center);
	}
	if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(
		MakeText(WidgetTree, FText::FromString(TEXT("Progress since your last save will be lost.")), 16,
			FLinearColor(0.7f, 0.78f, 0.88f, 1.f))))
	{
		S->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));
		S->SetHorizontalAlignment(HAlign_Center);
	}
	AddFlatButton(WidgetTree, Box, ConfirmLabel)->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnConfirmProceed);
	AddFlatButton(WidgetTree, Box, TEXT("BACK"))->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnConfirmBack);

	if (Root) { Root->SetContent(Box); }
}

void UPauseMenuWidget::OnConfirmProceed()
{
	ABridgePlayerController* C = Owner();
	if (!C) { return; }
	switch (PendingExit)
	{
	case EPendingExit::MainMenu: C->PauseMainMenu(); break;
	case EPendingExit::Quit:     C->PauseQuit();     break;
	default: break;
	}
}

void UPauseMenuWidget::OnConfirmBack()
{
	PendingExit = EPendingExit::None;
	if (Root && MainPanel) { Root->SetContent(MainPanel); }
}
