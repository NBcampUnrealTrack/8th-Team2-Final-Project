#include "UI/Marker/NUWorldMarkerWidget.h"

#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "NUGAS/NUGameplayTags.h"

UNUWorldMarkerWidget::UNUWorldMarkerWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PulseTags.AddTag(NU::Marker::QuestTurnIn);
	PulseTags.AddTag(NU::Marker::QuestTarget);
}

void UNUWorldMarkerWidget::SetMarkerTag(FGameplayTag NewTag)
{
	// 표시가 새로 생기거나 다른 표시로 바뀌면 톡 튀어나오게 한다 (같은 태그를 다시 넣을 때는 그대로)
	if (NewTag.IsValid() && NewTag != MarkerTag)
	{
		PopTime = 0.f;
	}
	MarkerTag = NewTag;
	RefreshMarkerImage();

	BP_OnMarkerTagChanged(MarkerTag);
}

void UNUWorldMarkerWidget::RefreshMarkerImage()
{
	if (IsValid(MarkerPulse))
	{
		const bool bPulse = PulseTags.HasTagExact(MarkerTag) || (bPulseOnOffer && MarkerTag == NU::Marker::QuestOffer);
		MarkerPulse->SetVisibility(bPulse ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (!IsValid(MarkerImage))
	{
		return;
	}

	const TObjectPtr<UTexture2D>* Texture = MarkerTextures.Find(MarkerTag);
	if (Texture == nullptr || !IsValid(Texture->Get()))
	{
		MarkerImage->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	MarkerImage->SetBrushFromTexture(Texture->Get(), false);
	const FLinearColor* MarkerTint = MarkerTints.Find(MarkerTag);
	MarkerImage->SetColorAndOpacity(MarkerTint != nullptr ? *MarkerTint : FLinearColor::White);
	MarkerImage->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UNUWorldMarkerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!IsValid(MarkerFloat))
	{
		return;
	}

	FloatTime = FMath::Fmod(FloatTime + InDeltaTime, FMath::Max(FloatPeriod, 0.1f));
	const float Wave = 0.5f - 0.5f * FMath::Cos(UE_TWO_PI * FloatTime / FMath::Max(FloatPeriod, 0.1f));

	// 튀어나오기: 0.6배에서 시작해 살짝 넘쳤다가(1.1) 제자리로, 0.4초
	float Scale = 1.f;
	if (PopTime >= 0.f)
	{
		PopTime += InDeltaTime;
		const float T = FMath::Clamp(PopTime / 0.4f, 0.f, 1.f);
		const float Back = 1.f + 2.70158f * FMath::Pow(T - 1.f, 3.f) + 1.70158f * FMath::Pow(T - 1.f, 2.f);
		Scale = FMath::Lerp(0.6f, 1.f, Back);
		if (T >= 1.f)
		{
			PopTime = -1.f;
			Scale = 1.f;
		}
	}

	FWidgetTransform Transform;
	Transform.Translation = FVector2D(0.f, -FloatAmplitude * Wave);
	Transform.Scale = FVector2D(Scale, Scale);
	MarkerFloat->SetRenderTransform(Transform);
}

void UNUWorldMarkerWidget::SetEdgeDirection(bool bOnEdge, float AngleDegrees)
{
	if (!IsValid(MarkerEdgePointer))
	{
		return;
	}

	MarkerEdgePointer->SetVisibility(bOnEdge ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bOnEdge)
	{
		// 꼭지 그림은 위를 가리킨다. 중심에서 그 방향으로 밀어 두고 같은 각도로 돌린다
		const float Radians = FMath::DegreesToRadians(AngleDegrees);
		FWidgetTransform Transform;
		Transform.Translation = FVector2D(FMath::Sin(Radians), -FMath::Cos(Radians)) * EdgePointerDistance;
		Transform.Angle = AngleDegrees;
		MarkerEdgePointer->SetRenderTransform(Transform);
	}
}
