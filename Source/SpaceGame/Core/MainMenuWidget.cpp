// SpaceGame — bridge simulator. Main menu implementation.

#include "Core/MainMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/MenuUI.h"
#include "Core/SpaceGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

using MenuUI::MakeText;

namespace
{
	// Encounter map opened by New Game / Continue.
	const TCHAR* const EncounterMap = TEXT("VSlice_Arena");

	// Menu buttons use a slightly larger vertical padding than the in-game overlays.
	UButton* AddMenuButton(UWidgetTree* Tree, UVerticalBox* Box, const FString& Label)
	{
		return MenuUI::AddFlatButton(Tree, Box, Label, 8.f);
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
	Root = WidgetTree->ConstructWidget<UBorder>();
	Root->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.06f, 1.f));
	Root->SetHorizontalAlignment(HAlign_Center);
	Root->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Root;

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	MainPanel = Box;
	Root->SetContent(Box);

	Box->AddChildToVerticalBox(MakeText(WidgetTree, FText::FromString(TEXT("S P A C E G A M E")), 52,
		FLinearColor(0.6f, 0.85f, 1.f, 1.f)));
	UTextBlock* Sub = MakeText(WidgetTree, FText::FromString(TEXT("BRIDGE SIMULATOR")), 20,
		FLinearColor(0.4f, 0.55f, 0.7f, 1.f));
	if (UVerticalBoxSlot* SubSlot = Box->AddChildToVerticalBox(Sub))
	{
		SubSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 40.f));
		SubSlot->SetHorizontalAlignment(HAlign_Center);
	}

	UButton* NewGameBtn = AddMenuButton(WidgetTree, Box, TEXT("NEW GAME"));
	NewGameBtn->OnClicked.AddDynamic(this, &UMainMenuWidget::OnNewGame);

	ContinueButton = AddMenuButton(WidgetTree, Box, TEXT("CONTINUE"));
	ContinueLabel = Cast<UTextBlock>(ContinueButton->GetChildAt(0));
	ContinueButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OnContinue);

	UButton* QuitBtn = AddMenuButton(WidgetTree, Box, TEXT("QUIT"));
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
	// Swap the menu to a ship-select panel; picking a ship starts the campaign.
	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();

	if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(
		MakeText(WidgetTree, FText::FromString(TEXT("SELECT YOUR SHIP")), 36, FLinearColor(0.6f, 0.85f, 1.f, 1.f))))
	{
		S->SetPadding(FMargin(0.f, 0.f, 0.f, 24.f));
		S->SetHorizontalAlignment(HAlign_Center);
	}

	AddMenuButton(WidgetTree, Box, TEXT("INTERCEPTOR  —  fast, agile, light hull"))
		->OnClicked.AddDynamic(this, &UMainMenuWidget::OnPickInterceptor);
	AddMenuButton(WidgetTree, Box, TEXT("CRUISER  —  slow, tough, hits hard"))
		->OnClicked.AddDynamic(this, &UMainMenuWidget::OnPickCruiser);
	AddMenuButton(WidgetTree, Box, TEXT("BACK"))
		->OnClicked.AddDynamic(this, &UMainMenuWidget::OnBack);

	if (Root) { Root->SetContent(Box); }
}

void UMainMenuWidget::OnPickInterceptor()
{
	if (USpaceGameInstance* GI = GetGameInstance<USpaceGameInstance>())
	{
		GI->SetPlayerShip(EPlayerShipType::Interceptor);
	}
	StartCampaign();
}

void UMainMenuWidget::OnPickCruiser()
{
	if (USpaceGameInstance* GI = GetGameInstance<USpaceGameInstance>())
	{
		GI->SetPlayerShip(EPlayerShipType::Cruiser);
	}
	StartCampaign();
}

void UMainMenuWidget::OnBack()
{
	if (Root && MainPanel) { Root->SetContent(MainPanel); }
}

void UMainMenuWidget::StartCampaign()
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
