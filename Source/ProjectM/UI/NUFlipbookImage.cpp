#include "UI/NUFlipbookImage.h"

#include "Engine/World.h"
#include "Field/NUSpriteFrameOffset.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PaperFlipbook.h"
#include "PaperSprite.h"
#include "TimerManager.h"

void UNUFlipbookImage::SetFlipbook(UPaperFlipbook* InFlipbook, float InDesiredHeight)
{
	Flipbook = InFlipbook;
	DesiredHeight = InDesiredHeight;

	UWorld* World = GetWorld();
	if (World != nullptr)
	{
		World->GetTimerManager().ClearTimer(FrameTimer);
	}

	if (Flipbook == nullptr || Flipbook->GetNumKeyFrames() <= 0)
	{
		SetBrush(FSlateBrush());
		return;
	}

	// 키프레임이 하나뿐이거나 재생 길이가 없으면 정지 이미지로 둔다
	if (World == nullptr || Flipbook->GetNumKeyFrames() <= 1 || Flipbook->GetTotalDuration() <= 0.f)
	{
		ShowFrame(0);
		return;
	}

	// 같은 틱에 건 레이어(몸·외모·머리)는 시작 시각이 같아 늘 같은 키프레임을 그린다
	StartTime = World->GetTimeSeconds();
	HandleFrameTimer();
}

void UNUFlipbookImage::SetFlipbookAtScale(UPaperFlipbook* InFlipbook, float InPixelScale)
{
	PixelScale = FMath::Max(InPixelScale, 0.f);
	SetFlipbook(InFlipbook, DesiredHeight);
}

void UNUFlipbookImage::SetTintMaterial(UMaterialInterface* InMaterial, const FLinearColor& InTint)
{
	TintMaterial = InMaterial;
	Tint = InTint;
	if (Flipbook != nullptr)
	{
		ShowFrame(ShownKeyFrame); // 지금 그림부터 바로 물든다
	}
}

void UNUFlipbookImage::FollowBodyBounce(const UPaperFlipbook* BodyFlipbook, float ExtraUpTexels)
{
	BounceOffsets = BodyFlipbook != nullptr ? NUSpriteFrameOffset::GetVerticalOffsets(BodyFlipbook) : TArray<float>();
	BounceExtraUpTexels = BodyFlipbook != nullptr ? ExtraUpTexels : 0.f;
	if (Flipbook != nullptr)
	{
		ShowFrame(ShownKeyFrame);
	}
}

void UNUFlipbookImage::StopAtKeyFrame(int32 KeyFrameIndex)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FrameTimer);
	}
	if (Flipbook != nullptr && Flipbook->GetNumKeyFrames() > 0)
	{
		ShowFrame(FMath::Clamp(KeyFrameIndex, 0, Flipbook->GetNumKeyFrames() - 1));
	}
}

void UNUFlipbookImage::BeginDestroy()
{
	// 타이머는 UObject 약참조라 놔둬도 안전하지만, 월드가 살아 있으면 즉시 정리한다
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FrameTimer);
	}
	Super::BeginDestroy();
}

void UNUFlipbookImage::ShowFrame(int32 KeyFrameIndex)
{
	UPaperSprite* Sprite = Flipbook != nullptr ? Flipbook->GetSpriteAtFrame(KeyFrameIndex) : nullptr;
	if (Sprite == nullptr)
	{
		return;
	}
	ShownKeyFrame = KeyFrameIndex;

	FSlateBrush NewBrush;
	NewBrush.SetResourceObject(Sprite);

	// 원본 비율 유지: 스프라이트 실제 크기를 DesiredHeight에 맞춰 스케일.
	// GetSourceDimensions는 텍스처 표면 크기를 곱해 재는데, 에디터에서 텍스처가
	// 아직 준비(비동기 컴파일) 중이면 자리표시 크기가 나와 첫 그리기의 배율이 틀어진다
	// (선택창 첫 진입에서 파츠만 커지던 문제). ImportedSize는 저장된 메타데이터라 늘 같다
	const FSlateAtlasData AtlasData = Sprite->GetSlateAtlasData();
	FVector2D SourceSize = AtlasData.GetSourceDimensions();
	if (const UTexture2D* Atlas = Cast<UTexture2D>(AtlasData.AtlasTexture))
	{
		const FIntPoint Imported = Atlas->GetImportedSize();
		SourceSize = FVector2D(Imported.X * AtlasData.SizeUV.X, Imported.Y * AtlasData.SizeUV.Y);
	}
	const float Scale = PixelScale > 0.f ? PixelScale : (SourceSize.Y > 0.f ? DesiredHeight / SourceSize.Y : 1.f);
	NewBrush.ImageSize = SourceSize * Scale;

	// 몸을 따라가는 층이면 몸 그림의 이 키프레임만큼 올린다 --- 남캐 직업 몸은 공용 머리와 흔들림이 달라 표가 0이 아니다.
	// 표는 스프라이트 유닛에 위가 + 이고, 화면은 그림 1칸이 Scale 픽셀에 아래가 + 다
	if (!BounceOffsets.IsEmpty() || BounceExtraUpTexels != 0.f || !GetRenderTransform().Translation.IsZero())
	{
		const float UpTexels = BounceExtraUpTexels
			+ (BounceOffsets.IsValidIndex(KeyFrameIndex) ? BounceOffsets[KeyFrameIndex] * Sprite->GetPixelsPerUnrealUnit() : 0.f);
		SetRenderTranslation(FVector2D(0.f, -UpTexels * Scale));
	}

	// 물들일 때는 스프라이트 대신 머티리얼을 그린다 --- 스프라이트가 시트의 어느 칸인지는 UVRect로 넘긴다
	if (TintMaterial != nullptr)
	{
		if (TintInstance == nullptr || TintInstance->Parent != TintMaterial)
		{
			TintInstance = UMaterialInstanceDynamic::Create(TintMaterial, this);
		}
		TintInstance->SetTextureParameterValue(TEXT("SpriteTexture"), AtlasData.AtlasTexture);
		TintInstance->SetVectorParameterValue(TEXT("UVRect"),
			FLinearColor(AtlasData.StartUV.X, AtlasData.StartUV.Y, AtlasData.SizeUV.X, AtlasData.SizeUV.Y));
		TintInstance->SetVectorParameterValue(TEXT("HairTint"), Tint);
		NewBrush.SetResourceObject(TintInstance);
	}

	SetBrush(NewBrush);
}

void UNUFlipbookImage::HandleFrameTimer()
{
	UWorld* World = GetWorld();
	if (Flipbook == nullptr || World == nullptr || Flipbook->GetNumKeyFrames() <= 0)
	{
		return;
	}

	// GetSpriteAtFrame은 키프레임 번호를 받는다 --- 키프레임이 여러 프레임을 차지하는 플립북(fps 100에 키프레임당 10~12프레임)을
	// 프레임 번호로 세면 앞 몇 프레임만 휙 돌고 나머지 동안 마지막 그림에 멈춘다 (9/17 선택창)
	const float Fps = Flipbook->GetFramesPerSecond();
	const float Elapsed = FMath::Fmod(static_cast<float>(World->GetTimeSeconds() - StartTime), Flipbook->GetTotalDuration());
	// 반 프레임 앞을 봐서 경계에 딱 걸린 시각이 앞 키프레임으로 떨어지지 않게 한다
	const int32 KeyFrame = Flipbook->GetKeyFrameIndexAtTime(Elapsed + 0.5f / Fps, true);
	ShowFrame(KeyFrame);

	int32 EndFrame = 0;
	for (int32 Index = 0; Index <= KeyFrame; ++Index)
	{
		EndFrame += Flipbook->GetKeyFrameChecked(Index).FrameRun;
	}
	const float Remaining = FMath::Max(EndFrame / Fps - Elapsed, 0.5f / Fps);
	World->GetTimerManager().SetTimer(FrameTimer,
		FTimerDelegate::CreateUObject(this, &UNUFlipbookImage::HandleFrameTimer),
		Remaining, false);
}
