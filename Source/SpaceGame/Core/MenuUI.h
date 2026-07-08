// SpaceGame — bridge simulator. Shared C++-UMG menu builders (M18).
//
// Small inline helpers for the code-built menus (main menu, pause, outcome). Shared so the
// per-file anonymous-namespace copies don't collide in unity builds, and so the menus keep a
// consistent flat dark-blue look.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace MenuUI
{
	inline UTextBlock* MakeText(UWidgetTree* Tree, const FText& Str, int32 Size, FLinearColor Color)
	{
		UTextBlock* T = Tree->ConstructWidget<UTextBlock>();
		T->SetText(Str);
		FSlateFontInfo Font = T->GetFont();
		Font.Size = Size;
		T->SetFont(Font);
		T->SetColorAndOpacity(FSlateColor(Color));
		T->SetJustification(ETextJustify::Center);
		return T;
	}

	/** A flat dark-blue button with a centred label as its first (and only) child. */
	inline UButton* MakeFlatButton(UWidgetTree* Tree, const FString& Label)
	{
		UButton* B = Tree->ConstructWidget<UButton>();
		B->SetBackgroundColor(FLinearColor(0.06f, 0.10f, 0.17f, 1.f));
		B->AddChild(MakeText(Tree, FText::FromString(Label), 20, FLinearColor(0.92f, 0.96f, 1.f, 1.f)));
		return B;
	}

	/** Add a full-width flat button to a vertical box with vertical padding PadY. */
	inline UButton* AddFlatButton(UWidgetTree* Tree, UVerticalBox* Box, const FString& Label, float PadY = 6.f)
	{
		UButton* B = MakeFlatButton(Tree, Label);
		if (UVerticalBoxSlot* Slot = Box->AddChildToVerticalBox(B))
		{
			Slot->SetPadding(FMargin(0.f, PadY));
			Slot->SetHorizontalAlignment(HAlign_Fill);
		}
		return B;
	}

	/** Add the keyboard-reference rows (R2 controls card). Shared by the main menu's CONTROLS
	 *  panel and the in-game H overlay so the two never drift apart. */
	inline void AddControlRows(UWidgetTree* Tree, UVerticalBox* Box)
	{
		auto AddRow = [Tree, Box](const TCHAR* Section, const TCHAR* Keys)
		{
			UTextBlock* S = MakeText(Tree, FText::FromString(Section), 14, FLinearColor(0.35f, 0.8f, 0.75f, 1.f));
			if (UVerticalBoxSlot* SS = Box->AddChildToVerticalBox(S))
			{
				SS->SetPadding(FMargin(0.f, 12.f, 0.f, 2.f));
				SS->SetHorizontalAlignment(HAlign_Center);
			}
			UTextBlock* K = MakeText(Tree, FText::FromString(Keys), 18, FLinearColor(0.85f, 0.92f, 1.f, 1.f));
			if (UVerticalBoxSlot* KS = Box->AddChildToVerticalBox(K))
			{
				KS->SetHorizontalAlignment(HAlign_Center);
			}
		};
		AddRow(TEXT("STATIONS"),          TEXT("1  Helm      2  Weapons      3  Engineering"));
		AddRow(TEXT("HELM  (1)"),         TEXT("W/S throttle     A/D turn     F dock / undock     R warp (along bow)     G lay in course"));
		AddRow(TEXT("WEAPONS  (2)"),      TEXT("RIGHT cycle target     SPACE fire beam     T fire torpedo"));
		AddRow(TEXT("ENGINEERING  (3)"),  TEXT("LEFT/RIGHT select system     UP/DOWN route power"));
		AddRow(TEXT("SCIENCE  (any)"),    TEXT("C cycle scan contact     X scan"));
		AddRow(TEXT("GENERAL"),           TEXT("H controls     ESC pause     crew consoles: any LAN browser"));
	}
}
