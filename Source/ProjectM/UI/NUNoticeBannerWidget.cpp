#include "UI/NUNoticeBannerWidget.h"

#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "TimerManager.h"

void UNUNoticeBannerWidget::ShowMessage(const FText& InMessage)
{
	Message = InMessage;

	MessageText->SetText(BalanceLines(Message));

	SetVisibility(ESlateVisibility::HitTestInvisible); // 아래 창의 클릭을 막지 않는다
	if (NoticeRoot != nullptr)
	{
		bLeaving = false;
		NoticeMotion.Add(NoticeRoot, 0.f, EnterSeconds, EnterOffset, 0.f, EnterFromScale);
		NoticeMotion.Start();
	}
	OnNoticeShown();

	// 연달아 오면 마지막 문구만 남기고 수명을 다시 센다
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimer);
		World->GetTimerManager().SetTimer(HideTimer, this, &UNUNoticeBannerWidget::HideBanner, Lifetime, false);
	}
}

FText UNUNoticeBannerWidget::BalanceLines(const FText& InMessage) const
{
	if (BalancedWrapWidth <= 0.f || !FSlateApplication::IsInitialized() || MessageText == nullptr)
	{
		return InMessage;
	}
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FSlateFontInfo FontInfo = MessageText->GetFont();
	auto Width = [&Measure, &FontInfo](const FString& S) { return static_cast<float>(Measure->Measure(S, FontInfo).X); };

	const FString Source = InMessage.ToString().TrimStartAndEnd();
	const float Full = Width(Source);
	if (Full <= BalancedWrapWidth)
	{
		return InMessage;
	}
	TArray<FString> Words;
	Source.ParseIntoArray(Words, TEXT(" "), true);
	if (Words.Num() < 2)
	{
		return InMessage;           // 띄어쓰기 없는 긴 말은 WBP 의 자동 줄바꿈에 맡긴다
	}

	const int32 LineCount = FMath::CeilToInt(Full / BalancedWrapWidth);
	TArray<FString> Lines;
	if (LineCount == 2)
	{
		// 두 줄: 들어맞는 나눔 중 문장 끝에서 끊는 것을 먼저, 그다음 긴 줄이 가장 짧은 것
		float Best = TNumericLimits<float>::Max();
		for (int32 Split = 1; Split < Words.Num(); ++Split)
		{
			const FString A = FString::Join(TArrayView<const FString>(Words.GetData(), Split), TEXT(" "));
			const FString B = FString::Join(TArrayView<const FString>(Words.GetData() + Split, Words.Num() - Split), TEXT(" "));
			const float Longer = FMath::Max(Width(A), Width(B));
			if (Longer > BalancedWrapWidth)
			{
				continue;
			}
			const TCHAR Last = A[A.Len() - 1];
			const bool bSentenceEnd = Last == TEXT('.') || Last == TEXT('!') || Last == TEXT('?') || Last == TEXT('~') || Last == TEXT(',') || Last == 0x2026;
			const float Cost = Longer - (bSentenceEnd ? 100000.f : 0.f);
			if (Cost < Best)
			{
				Best = Cost;
				Lines = { A, B };
			}
		}
	}
	if (Lines.IsEmpty())
	{
		// 세 줄 이상(또는 두 줄 나눔이 안 맞을 때): 고른 폭을 목표로 앞에서부터 채운다
		const float Target = FMath::Min(BalancedWrapWidth, Full / LineCount * 1.06f);
		FString Current;
		for (const FString& Word : Words)
		{
			const FString Try = Current.IsEmpty() ? Word : Current + TEXT(" ") + Word;
			if (!Current.IsEmpty() && Width(Try) > Target)
			{
				Lines.Add(Current);
				Current = Word;
			}
			else
			{
				Current = Try;
			}
		}
		Lines.Add(Current);
	}
	if (MaxLines > 0 && Lines.Num() > MaxLines)
	{
		Lines.SetNum(MaxLines);
		FString& Tail = Lines.Last();
		while (!Tail.IsEmpty() && Width(Tail + TEXT("…")) > BalancedWrapWidth)
		{
			Tail.LeftChopInline(1);
		}
		Tail = Tail.TrimEnd() + TEXT("…");
	}
	return FText::FromString(FString::Join(Lines, TEXT("\n")));
}

void UNUNoticeBannerWidget::HideBanner()
{
	if (NoticeRoot != nullptr && GetVisibility() != ESlateVisibility::Collapsed)
	{
		bLeaving = true;
		NoticeMotion.AddOut(NoticeRoot, 0.f, 0.3f, FVector2D(0.f, -8.f));
		NoticeMotion.Start();
		return;
	}
	SetVisibility(ESlateVisibility::Collapsed);
}

void UNUNoticeBannerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!NoticeMotion.Advance(InDeltaTime) && bLeaving)
	{
		bLeaving = false;
		SetVisibility(ESlateVisibility::Collapsed);
		NoticeRoot->SetRenderOpacity(1.f);
		NoticeRoot->SetRenderTransform(FWidgetTransform());
	}
}

void UNUNoticeBannerWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimer);
	}
	Super::NativeDestruct();
}
