#include "UI/NUScreenMotion.h"

#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "HAL/IConsoleManager.h"

// 연출 속도 확인용 (1 = 보통, 0.1 = 열 배 느리게). 출시 빌드에서도 1이면 아무 일도 없다
static TAutoConsoleVariable<float> CVarNUUIMotionScale(TEXT("nu.UI.MotionScale"), 1.f,
	TEXT("UI 등장 연출(FNUScreenMotion) 시간 배율. 0.1이면 열 배 느리게 재생해 프레임을 확인할 수 있다"));

void FNUScreenMotion::Add(UWidget* Widget, float Delay, float Duration, const FVector2D& Offset, float FromOpacity, float FromScale)
{
	if (!IsValid(Widget))
	{
		return;
	}

	// 같은 위젯을 다시 넣으면 이전 것을 끝내고 그 끝 모습을 원래 값으로 삼는다 --- 원래 값이 중간 모습으로 바뀌면 안 된다
	if (FEntry* Existing = Entries.FindByPredicate([Widget](const FEntry& Entry) { return Entry.Widget == Widget; }))
	{
		if (Existing->bOut)
		{
			// 사라지던 중이면 사라지기 전 모습을 원래 값으로 삼는다 (흐려진 모습이 원래 값이 되면 다시 나타나지 못한다)
			Widget->SetRenderTransform(Existing->Rest);
			Widget->SetRenderOpacity(Existing->RestOpacity);
		}
		else
		{
			Apply(*Existing, 1.f);
		}
		Entries.RemoveAll([Widget](const FEntry& Entry) { return Entry.Widget == Widget; });
	}

	FEntry& Entry = Entries.AddDefaulted_GetRef();
	Entry.Widget = Widget;
	Entry.Delay = Elapsed + FMath::Max(Delay, 0.f);   // 진행 중에 보탠 것은 지금부터 잰다
	Entry.Duration = FMath::Max(Duration, 0.01f);
	Entry.Offset = Offset;
	Entry.FromOpacity = FromOpacity;
	Entry.FromScale = FromScale;
	Entry.Rest = Widget->GetRenderTransform();
	Entry.RestOpacity = Widget->GetRenderOpacity();
}

void FNUScreenMotion::AddPulse(UWidget* Widget, float Delay, float Duration, float PeakOpacity, float FromScale)
{
	Add(Widget, Delay, Duration, FVector2D::ZeroVector, PeakOpacity, FromScale);
	if (Entries.Num() > 0 && Entries.Last().Widget == Widget)
	{
		Entries.Last().bPulse = true;
	}
}

void FNUScreenMotion::AddOut(UWidget* Widget, float Delay, float Duration, const FVector2D& Offset, float ToOpacity)
{
	Add(Widget, Delay, Duration, Offset, ToOpacity, 1.f);
	if (Entries.Num() > 0 && Entries.Last().Widget == Widget)
	{
		Entries.Last().bOut = true;
	}
}

void FNUScreenMotion::AddChildren(UWidget* Panel, float FirstDelay, float Step, float Duration, const FVector2D& Offset)
{
	const UPanelWidget* Container = Cast<UPanelWidget>(Panel);
	if (Container == nullptr)
	{
		return;
	}
	int32 Order = 0;
	for (int32 Index = 0; Index < Container->GetChildrenCount(); ++Index)
	{
		UWidget* Child = Container->GetChildAt(Index);
		// 접힌 줄은 자리를 차지하지 않으니 순서도 받지 않는다 (보이는 줄끼리 고르게 이어진다)
		if (Child == nullptr || Child->GetVisibility() == ESlateVisibility::Collapsed)
		{
			continue;
		}
		Add(Child, FirstDelay + Step * Order++, Duration, Offset);
	}
}

void FNUScreenMotion::Start()
{
	// 진행 중에 보탠 것도 있으므로 처음으로 돌리지 않고 지금 시각의 모습을 그린다 (막 보탠 것은 시작 모습이다)
	bRunning = !Entries.IsEmpty();
	for (const FEntry& Entry : Entries)
	{
		Apply(Entry, FMath::Clamp((Elapsed - Entry.Delay) / Entry.Duration, 0.f, 1.f));
	}
}

bool FNUScreenMotion::Advance(float DeltaTime)
{
	if (!bRunning)
	{
		return false;
	}
	DeltaTime *= FMath::Max(CVarNUUIMotionScale.GetValueOnGameThread(), 0.f);

	// 첫 틱은 화면이 막 열려 길게 잡히기 쉽다(레벨 로딩 직후) --- 한 번에 1/20초 넘게 가지 않게 해 첫 장면을 건너뛰지 않는다
	Elapsed += FMath::Min(DeltaTime, 0.05f);
	bool bAnyLeft = false;
	for (const FEntry& Entry : Entries)
	{
		const float Alpha = FMath::Clamp((Elapsed - Entry.Delay) / Entry.Duration, 0.f, 1.f);
		Apply(Entry, Alpha);
		bAnyLeft |= Alpha < 1.f;
	}
	if (!bAnyLeft)
	{
		Entries.Reset();
		Elapsed = 0.f;
		bRunning = false;
	}
	return bRunning;
}

void FNUScreenMotion::Finish()
{
	for (const FEntry& Entry : Entries)
	{
		Apply(Entry, 1.f);
	}
	Entries.Reset();
	Elapsed = 0.f;
	bRunning = false;
}

void FNUScreenMotion::Apply(const FEntry& Entry, float Alpha)
{
	UWidget* Widget = Entry.Widget.Get();
	if (Widget == nullptr)
	{
		return;
	}

	if (Entry.bPulse)
	{
		// 올랐다 내려오는 한 번의 반짝임. 끝나면 정확히 원래 모습으로 (크기가 FromScale에 남지 않게)
		if (Alpha <= 0.f || Alpha >= 1.f)
		{
			Widget->SetRenderTransform(Entry.Rest);
			Widget->SetRenderOpacity(Entry.RestOpacity);
			return;
		}
		const float Wave = FMath::Sin(PI * FMath::Pow(Alpha, 0.7f));     // 빨리 차고 천천히 꺼진다
		FWidgetTransform Transform = Entry.Rest;
		Transform.Scale *= FMath::Lerp(Entry.FromScale, 1.f, Wave);
		Widget->SetRenderTransform(Transform);
		Widget->SetRenderOpacity(FMath::Lerp(Entry.RestOpacity, Entry.FromOpacity, Wave));
		return;
	}

	if (Entry.bOut)
	{
		// 천천히 떠나 빨리 사라진다 (ease-in). 끝나도 원래 모습으로 돌아가지 않는다
		const float Leave = Alpha * Alpha;
		FWidgetTransform Transform = Entry.Rest;
		Transform.Translation += Entry.Offset * Leave;
		Widget->SetRenderTransform(Transform);
		Widget->SetRenderOpacity(FMath::Lerp(Entry.RestOpacity, Entry.FromOpacity, Alpha));
		return;
	}

	// 위치는 빨리 붙고 천천히 멈춘다 (ease-out cubic), 불투명도는 그보다 조금 빨리 찬다
	const float Move = 1.f - FMath::Pow(1.f - Alpha, 3.f);
	const float Fade = 1.f - FMath::Pow(1.f - Alpha, 2.f);

	FWidgetTransform Transform = Entry.Rest;
	Transform.Translation += Entry.Offset * (1.f - Move);
	Transform.Scale *= FMath::Lerp(Entry.FromScale, 1.f, Move);
	Widget->SetRenderTransform(Transform);
	Widget->SetRenderOpacity(FMath::Lerp(Entry.FromOpacity, Entry.RestOpacity, Fade));
}
