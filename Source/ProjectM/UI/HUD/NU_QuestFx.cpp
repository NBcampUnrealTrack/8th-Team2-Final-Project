#include "UI/HUD/NU_QuestFx.h"

#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "UI/HUD/NU_HudBattleFold.h"
#include "UObject/UObjectIterator.h"
#include "WorldLocation/UI/NUWorldLocationWidget.h"

namespace NUQuestFx
{
	constexpr int32 SparkCount = 8;
	constexpr float OutDuration = 0.45f;
	// 줄에 들어온 띠를 이만큼 지켜본 뒤 시작한다 --- 도착 목표는 퀘스트 이벤트가 지역 배너보다 한 틱 먼저 올 수 있다
	constexpr double SettleSeconds = 0.3;

	// HUD 팔레트 (선형). 금 = 수락·완료, 청록 = 가야 할 곳(보고)
	const FLinearColor Gold(0.905f, 0.680f, 0.270f, 1.f);
	const FLinearColor Teal(0.212f, 0.745f, 0.672f, 1.f);
	const FLinearColor Ivory(0.880f, 0.846f, 0.791f, 1.f);
	const FLinearColor TitleGold(1.000f, 0.800f, 0.430f, 1.f);

	UWidget* Find(UUserWidget* Owner, const FString& Name)
	{
		return Owner->GetWidgetFromName(FName(*Name));
	}

	// 지역 진입 배너가 떠 있는지. "○○에 도착하기" 목표는 도착 순간 진입 배너와 완료 띠가 같이 뜬다 --- 배너가 끝난 뒤에 띄운다
	bool IsAreaBannerShowing(const UUserWidget* Owner)
	{
		for (TObjectIterator<UNUWorldLocationWidget> It; It; ++It)
		{
			if (It->GetWorld() == Owner->GetWorld() && It->IsInViewport() && It->IsBannerShowing())
			{
				return true;
			}
		}
		return false;
	}
}

float FNUQuestFx::GetLength(EKind Kind)
{
	switch (Kind)
	{
	case EKind::Completed: return 3.2f;
	case EKind::Ready: return 2.6f;
	default: return 2.8f;
	}
}

int32 FNUQuestFx::Queue(EKind Kind, const FText& Title, const FText& Detail)
{
	FItem& Item = Pending.AddDefaulted_GetRef();
	Item.Kind = Kind;
	Item.Title = Title;
	Item.Detail = Detail;
	Item.Id = ++NextId;
	Item.QueuedAt = FPlatformTime::Seconds();
	return Item.Id;
}

void FNUQuestFx::Tick(UUserWidget* Owner, float DeltaTime)
{
	if (Owner == nullptr)
	{
		return;
	}

	if (Time >= 0.f)
	{
		Time += DeltaTime;
		const bool bIn = In.Advance(DeltaTime);
		const bool bOut = Out.Advance(DeltaTime);
		if (!bIn && !bOut)
		{
			Hide(Owner);
		}
		return;
	}

	// 전투 화면이 떠 있는 동안은 기다린다 (퀘스트 위젯 루트가 접혀 있어 보이지 않는다). 지역 진입 배너가 떠 있어도 기다린다 (같은 화면 위쪽)
	if (!Pending.IsEmpty() && FPlatformTime::Seconds() - Pending[0].QueuedAt >= NUQuestFx::SettleSeconds
		&& !FNUHudBattleFold::IsBattleUIActive(Owner) && !NUQuestFx::IsAreaBannerShowing(Owner))
	{
		const FItem Item = Pending[0];
		Pending.RemoveAt(0);
		LastStartedId = Item.Id;
		Start(Owner, Item);
	}
}

void FNUQuestFx::Reset(UUserWidget* Owner)
{
	LastStartedId = NextId;   // 기다리던 것도 "뜬 것"으로 친다 (보상 창이 영영 안 뜨지 않게)
	Pending.Reset();
	In.Finish();
	Out.Finish();
	if (Owner != nullptr)
	{
		Hide(Owner);
	}
}

void FNUQuestFx::Hide(UUserWidget* Owner)
{
	Time = -1.f;
	if (UWidget* Root = NUQuestFx::Find(Owner, TEXT("QuestFxRoot")))
	{
		Root->SetVisibility(ESlateVisibility::Collapsed);
		Root->SetRenderTransform(FWidgetTransform());
		Root->SetRenderOpacity(1.f);
	}
}

void FNUQuestFx::Start(UUserWidget* Owner, const FItem& Item)
{
	using namespace NUQuestFx;
	UWidget* Root = Find(Owner, TEXT("QuestFxRoot"));
	if (Root == nullptr)
	{
		return; // 띠가 없는 WBP
	}

	In.Finish();
	Out.Finish();
	Root->SetRenderTransform(FWidgetTransform());
	Root->SetRenderOpacity(1.f);

	const bool bDone = Item.Kind == EKind::Completed;
	const FLinearColor Accent = Item.Kind == EKind::Ready ? Teal : Gold;

	if (UTextBlock* Cap = Cast<UTextBlock>(Find(Owner, TEXT("QuestFxCap"))))
	{
		switch (Item.Kind)
		{
		case EKind::Ready: Cap->SetText(NSLOCTEXT("NUQuestFx", "Ready", "목표 달성")); break;
		case EKind::Completed: Cap->SetText(NSLOCTEXT("NUQuestFx", "Completed", "의뢰 완료")); break;
		default: Cap->SetText(NSLOCTEXT("NUQuestFx", "Accepted", "새 의뢰")); break;
		}
		Cap->SetColorAndOpacity(FSlateColor(Accent));
	}
	if (UTextBlock* Title = Cast<UTextBlock>(Find(Owner, TEXT("QuestFxTitle"))))
	{
		Title->SetText(Item.Title);
		Title->SetColorAndOpacity(FSlateColor(bDone ? TitleGold : Ivory));
	}
	UWidget* SubWidget = Find(Owner, TEXT("QuestFxSub"));
	if (UTextBlock* Sub = Cast<UTextBlock>(SubWidget))
	{
		Sub->SetText(Item.Detail);
		Sub->SetVisibility(Item.Detail.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	for (const TCHAR* Name : {TEXT("QuestFxCapDiamondL"), TEXT("QuestFxCapDiamondR")})
	{
		if (UImage* Diamond = Cast<UImage>(Find(Owner, Name)))
		{
			Diamond->SetColorAndOpacity(Accent);
		}
	}

	UWidget* Rays = Find(Owner, TEXT("QuestFxRays"));
	UWidget* Ring = Find(Owner, TEXT("QuestFxRing"));
	for (UWidget* Burst : {Rays, Ring})
	{
		if (Burst != nullptr)
		{
			Burst->SetVisibility(bDone ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			Burst->SetRenderTransform(FWidgetTransform());
			Burst->SetRenderOpacity(0.f);
		}
	}

	Root->SetVisibility(ESlateVisibility::HitTestInvisible);

	In.Add(Find(Owner, TEXT("QuestFxGlow")), 0.f, 0.45f, FVector2D::ZeroVector, 0.f, 0.7f);
	In.Add(Find(Owner, TEXT("QuestFxCapRow")), 0.02f, 0.45f, FVector2D(0.f, -10.f));
	if (bDone)
	{
		// 도장: 크게 찍혔다가 제자리로
		In.Add(Find(Owner, TEXT("QuestFxTitleRow")), 0.08f, 0.42f, FVector2D::ZeroVector, 0.f, 1.35f);
		In.AddPulse(Rays, 0.06f, 2.2f, 0.95f, 0.55f);
		if (Ring != nullptr)
		{
			In.Add(Ring, 0.08f, 0.9f, FVector2D::ZeroVector, 1.f, 0.3f);    // 평소 불투명도 0: 작게 시작해 커지며 사라진다
		}
	}
	else
	{
		In.Add(Find(Owner, TEXT("QuestFxTitleRow")), 0.1f, 0.5f, FVector2D(0.f, 14.f), 0.f, 0.92f);
	}
	In.Add(Find(Owner, TEXT("QuestFxRuleL")), 0.22f, 0.55f, FVector2D(56.f, 0.f));
	In.Add(Find(Owner, TEXT("QuestFxRuleR")), 0.22f, 0.55f, FVector2D(-56.f, 0.f));
	In.Add(SubWidget, 0.24f, 0.5f, FVector2D(0.f, 8.f));

	const int32 Sparks = bDone ? SparkCount : SparkCount / 2;
	for (int32 Index = 0; Index < SparkCount; ++Index)
	{
		UWidget* Spark = Find(Owner, FString::Printf(TEXT("QuestFxSpark%d"), Index));
		UWidget* SparkImage = Find(Owner, FString::Printf(TEXT("QuestFxSpark%dImg"), Index));
		if (Spark == nullptr || SparkImage == nullptr)
		{
			continue;
		}
		Spark->SetVisibility(Index < Sparks ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (Index >= Sparks)
		{
			continue;
		}
		if (UImage* Image = Cast<UImage>(SparkImage))
		{
			Image->SetColorAndOpacity(Item.Kind == EKind::Ready ? FLinearColor(0.70f, 1.f, 0.95f, 1.f) : FLinearColor::White);
		}
		// 캔버스 자리가 끝 위치: 가운데 가까이서 출발해 바깥으로 날아가며 반짝였다 꺼진다
		const UCanvasPanelSlot* SparkSlot = Cast<UCanvasPanelSlot>(Spark->Slot);
		const FVector2D Rest = SparkSlot != nullptr ? SparkSlot->GetPosition() : FVector2D::ZeroVector;
		const float Delay = 0.12f + 0.035f * Index;
		In.Add(Spark, Delay, 0.95f, -Rest * 0.8f, 1.f, 1.f);
		In.AddPulse(SparkImage, Delay, 1.1f, 1.f, 0.35f);
	}

	Length = GetLength(Item.Kind);
	Out.AddOut(Root, Length - OutDuration, OutDuration, FVector2D(0.f, -14.f));
	In.Start();
	Out.Start();
	Time = 0.f;
}
