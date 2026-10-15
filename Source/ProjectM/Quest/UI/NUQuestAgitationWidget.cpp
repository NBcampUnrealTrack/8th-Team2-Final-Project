#include "Quest/UI/NUQuestAgitationWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UNUQuestAgitationWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// 필드와 전투 모두 수치·상태 문구 없이 게이지만 표시한다.
	if (AgitationText != nullptr)
	{
		AgitationText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UNUQuestAgitationWidget::SetAgitation(float Value, float Maximum)
{
	MaxAgitation = FMath::Max(Maximum, 0.f);
	Agitation = FMath::Clamp(Value, 0.f, MaxAgitation);
	if (AgitationBar != nullptr) { AgitationBar->SetPercent(MaxAgitation > 0.f ? Agitation / MaxAgitation : 0.f); }
}
