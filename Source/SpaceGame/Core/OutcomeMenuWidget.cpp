// SpaceGame — bridge simulator. Outcome overlay implementation.

#include "Core/OutcomeMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/BridgePlayerController.h"
#include "Core/MenuUI.h"

using MenuUI::MakeText;
using MenuUI::AddFlatButton;

void UOutcomeMenuWidget::Setup(const FText& Title, FLinearColor TitleColor, const FText& Subtitle,
	const FString& PrimaryLabel, const FString& SecondaryLabel)
{
	CfgTitle = Title;
	CfgColor = TitleColor;
	CfgSubtitle = Subtitle;
	CfgPrimary = PrimaryLabel;
	CfgSecondary = SecondaryLabel;
}

TSharedRef<SWidget> UOutcomeMenuWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildUI();
	}
	return Super::RebuildWidget();
}

void UOutcomeMenuWidget::BuildUI()
{
	UBorder* Root = WidgetTree->ConstructWidget<UBorder>();
	Root->SetBrushColor(FLinearColor(0.01f, 0.02f, 0.04f, 0.9f));
	Root->SetHorizontalAlignment(HAlign_Center);
	Root->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Root;

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	Root->SetContent(Box);

	if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(MakeText(WidgetTree, CfgTitle, 46, CfgColor)))
	{
		S->SetHorizontalAlignment(HAlign_Center);
	}
	UTextBlock* Sub = MakeText(WidgetTree, CfgSubtitle, 18, FLinearColor(0.65f, 0.75f, 0.9f, 1.f));
	Sub->SetAutoWrapText(true);          // let a longer story epilogue wrap instead of overflowing
	Sub->SetWrapTextAt(760.f);
	if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Sub))
	{
		S->SetPadding(FMargin(0.f, 6.f, 0.f, 30.f));
		S->SetHorizontalAlignment(HAlign_Center);
	}

	if (!CfgPrimary.IsEmpty())
	{
		AddFlatButton(WidgetTree, Box, CfgPrimary)->OnClicked.AddDynamic(this, &UOutcomeMenuWidget::OnPrimary);
	}
	if (!CfgSecondary.IsEmpty())
	{
		AddFlatButton(WidgetTree, Box, CfgSecondary)->OnClicked.AddDynamic(this, &UOutcomeMenuWidget::OnSecondary);
	}
}

ABridgePlayerController* UOutcomeMenuWidget::Owner() const
{
	return Cast<ABridgePlayerController>(GetOwningPlayer());
}

void UOutcomeMenuWidget::OnPrimary()   { if (ABridgePlayerController* C = Owner()) { C->OutcomePrimary(); } }
void UOutcomeMenuWidget::OnSecondary() { if (ABridgePlayerController* C = Owner()) { C->OutcomeSecondary(); } }
