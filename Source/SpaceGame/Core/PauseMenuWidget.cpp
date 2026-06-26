// SpaceGame — bridge simulator. Pause overlay implementation.

#include "Core/PauseMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/BridgePlayerController.h"

namespace
{
	UTextBlock* MakeText(UWidgetTree* Tree, const FString& Str, int32 Size, FLinearColor Color)
	{
		UTextBlock* T = Tree->ConstructWidget<UTextBlock>();
		T->SetText(FText::FromString(Str));
		FSlateFontInfo Font = T->GetFont();
		Font.Size = Size;
		T->SetFont(Font);
		T->SetColorAndOpacity(FSlateColor(Color));
		T->SetJustification(ETextJustify::Center);
		return T;
	}

	UButton* AddButton(UWidgetTree* Tree, UVerticalBox* Box, const FString& Label)
	{
		UButton* B = Tree->ConstructWidget<UButton>();
		B->SetBackgroundColor(FLinearColor(0.06f, 0.10f, 0.17f, 1.f));
		B->AddChild(MakeText(Tree, Label, 20, FLinearColor(0.92f, 0.96f, 1.f, 1.f)));
		if (UVerticalBoxSlot* Slot = Box->AddChildToVerticalBox(B))
		{
			Slot->SetPadding(FMargin(0.f, 6.f));
			Slot->SetHorizontalAlignment(HAlign_Fill);
		}
		return B;
	}
}

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
	UBorder* Root = WidgetTree->ConstructWidget<UBorder>();
	Root->SetBrushColor(FLinearColor(0.01f, 0.02f, 0.04f, 0.85f)); // dim the paused scene
	Root->SetHorizontalAlignment(HAlign_Center);
	Root->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Root;

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	Root->SetContent(Box);

	if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(
		MakeText(WidgetTree, TEXT("PAUSED"), 40, FLinearColor(0.6f, 0.85f, 1.f, 1.f))))
	{
		S->SetPadding(FMargin(0.f, 0.f, 0.f, 24.f));
		S->SetHorizontalAlignment(HAlign_Center);
	}

	AddButton(WidgetTree, Box, TEXT("RESUME"))->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnResume);
	AddButton(WidgetTree, Box, TEXT("SAVE"))->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnSave);
	AddButton(WidgetTree, Box, TEXT("RESTART"))->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnRestart);
	AddButton(WidgetTree, Box, TEXT("MAIN MENU"))->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnMainMenu);
	AddButton(WidgetTree, Box, TEXT("QUIT"))->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnQuit);

	Toast = MakeText(WidgetTree, TEXT(""), 16, FLinearColor(0.4f, 1.f, 0.55f, 1.f));
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

void UPauseMenuWidget::OnResume()   { if (ABridgePlayerController* C = Owner()) { C->PauseResume(); } }
void UPauseMenuWidget::OnSave()     { if (ABridgePlayerController* C = Owner()) { C->PauseSave(); } }
void UPauseMenuWidget::OnRestart()  { if (ABridgePlayerController* C = Owner()) { C->PauseRestart(); } }
void UPauseMenuWidget::OnMainMenu() { if (ABridgePlayerController* C = Owner()) { C->PauseMainMenu(); } }
void UPauseMenuWidget::OnQuit()     { if (ABridgePlayerController* C = Owner()) { C->PauseQuit(); } }
