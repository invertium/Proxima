// SpaceGame — bridge simulator. Main menu implementation.

#include "Core/MainMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/SpaceGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

namespace
{
	// Encounter map opened by New Game / Continue.
	const TCHAR* const EncounterMap = TEXT("VSlice_Arena");

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

	// A flat dark-blue button with centred label; returns the button + (optionally) its label.
	UButton* MakeButton(UWidgetTree* Tree, UVerticalBox* Box, const FString& Label,
		UTextBlock** OutLabel = nullptr)
	{
		UButton* B = Tree->ConstructWidget<UButton>();
		B->SetBackgroundColor(FLinearColor(0.06f, 0.10f, 0.17f, 1.f));
		UTextBlock* T = MakeText(Tree, Label, 22, FLinearColor(0.92f, 0.96f, 1.f, 1.f));
		B->AddChild(T);
		if (UVerticalBoxSlot* Slot = Box->AddChildToVerticalBox(B))
		{
			Slot->SetPadding(FMargin(0.f, 8.f));
			Slot->SetHorizontalAlignment(HAlign_Fill);
		}
		if (OutLabel) { *OutLabel = T; }
		return B;
	}
}

TSharedRef<SWidget> UMainMenuWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildUI();
	}
	return Super::RebuildWidget();
}

void UMainMenuWidget::BuildUI()
{
	UBorder* Root = WidgetTree->ConstructWidget<UBorder>();
	Root->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.06f, 1.f));
	Root->SetHorizontalAlignment(HAlign_Center);
	Root->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Root;

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	Root->SetContent(Box);

	Box->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("S P A C E G A M E"), 52,
		FLinearColor(0.6f, 0.85f, 1.f, 1.f)));
	UTextBlock* Sub = MakeText(WidgetTree, TEXT("BRIDGE SIMULATOR"), 20, FLinearColor(0.4f, 0.55f, 0.7f, 1.f));
	if (UVerticalBoxSlot* SubSlot = Box->AddChildToVerticalBox(Sub))
	{
		SubSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 40.f));
		SubSlot->SetHorizontalAlignment(HAlign_Center);
	}

	UButton* NewGameBtn = MakeButton(WidgetTree, Box, TEXT("NEW GAME"));
	NewGameBtn->OnClicked.AddDynamic(this, &UMainMenuWidget::OnNewGame);

	UTextBlock* ContinueText = nullptr;
	ContinueButton = MakeButton(WidgetTree, Box, TEXT("CONTINUE"), &ContinueText);
	ContinueLabel = ContinueText;
	ContinueButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OnContinue);

	UButton* QuitBtn = MakeButton(WidgetTree, Box, TEXT("QUIT"));
	QuitBtn->OnClicked.AddDynamic(this, &UMainMenuWidget::OnQuit);
}

void UMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Grey out Continue when there's no save to resume.
	const USpaceGameInstance* GI = GetGameInstance<USpaceGameInstance>();
	const bool bHasSave = GI && GI->HasSave();
	if (ContinueButton) { ContinueButton->SetIsEnabled(bHasSave); }
	if (ContinueLabel)
	{
		ContinueLabel->SetColorAndOpacity(FSlateColor(bHasSave
			? FLinearColor(0.92f, 0.96f, 1.f, 1.f)
			: FLinearColor(0.4f, 0.45f, 0.55f, 1.f)));
	}
}

void UMainMenuWidget::OnNewGame()
{
	if (USpaceGameInstance* GI = GetGameInstance<USpaceGameInstance>())
	{
		GI->ResetCampaign();
	}
	UGameplayStatics::OpenLevel(this, FName(EncounterMap));
}

void UMainMenuWidget::OnContinue()
{
	USpaceGameInstance* GI = GetGameInstance<USpaceGameInstance>();
	if (GI && GI->LoadCampaign())
	{
		UGameplayStatics::OpenLevel(this, FName(EncounterMap));
	}
}

void UMainMenuWidget::OnQuit()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
