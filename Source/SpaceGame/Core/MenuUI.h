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
}
