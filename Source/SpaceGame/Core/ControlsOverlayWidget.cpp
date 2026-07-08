// SpaceGame — bridge simulator. Controls reference overlay implementation.

#include "Core/ControlsOverlayWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/MenuUI.h"

using MenuUI::MakeText;

TSharedRef<SWidget> UControlsOverlayWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildUI();
	}
	return Super::RebuildWidget();
}

void UControlsOverlayWidget::BuildUI()
{
	UBorder* Root = WidgetTree->ConstructWidget<UBorder>();
	Root->SetBrushColor(FLinearColor(0.01f, 0.02f, 0.04f, 0.82f)); // dim, but game keeps running
	Root->SetHorizontalAlignment(HAlign_Center);
	Root->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Root;

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	Root->SetContent(Box);

	if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(
		MakeText(WidgetTree, FText::FromString(TEXT("CONTROLS")), 34, FLinearColor(0.6f, 0.85f, 1.f, 1.f))))
	{
		S->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		S->SetHorizontalAlignment(HAlign_Center);
	}

	MenuUI::AddControlRows(WidgetTree, Box);

	if (UVerticalBoxSlot* F = Box->AddChildToVerticalBox(
		MakeText(WidgetTree, FText::FromString(TEXT("H to close")), 14, FLinearColor(0.5f, 0.6f, 0.7f, 1.f))))
	{
		F->SetPadding(FMargin(0.f, 20.f, 0.f, 0.f));
		F->SetHorizontalAlignment(HAlign_Center);
	}
}
