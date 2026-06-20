// SpaceGame — bridge simulator. End-of-encounter overlay implementation.

#include "Core/EndScreenWidget.h"

#include "Components/TextBlock.h"

void UEndScreenWidget::SetOutcome(const FText& Title, const FText& Subtitle, FLinearColor TitleColor)
{
	if (TitleText)
	{
		TitleText->SetText(Title);
		TitleText->SetColorAndOpacity(FSlateColor(TitleColor));
	}
	if (SubtitleText)
	{
		SubtitleText->SetText(Subtitle);
	}
}
