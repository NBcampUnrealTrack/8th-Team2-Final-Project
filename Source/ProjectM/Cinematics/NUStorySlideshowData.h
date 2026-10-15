#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Fonts/SlateFontInfo.h"
#include "NUStorySlideshowData.generated.h"

class ULevelSequence;
class USoundMix;
class UTexture2D;

// 사진과 대사 원문을 보관한다. 재생 상태는 보관하지 않는다.
USTRUCT(BlueprintType)
struct FNUStorySlide
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Story")
	TObjectPtr<UTexture2D> Image;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Story")
	TArray<FText> Lines;
};

UCLASS(BlueprintType)
class PROJECTM_API UNUStorySlideshowData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Story")
	TArray<FNUStorySlide> Slides;

	// 퀘스트 연출로 재생할 때 사용하는 Level Sequence. 미리보기 GameMode는 자체 설정을 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Story")
	TSoftObjectPtr<ULevelSequence> Sequence;

	// 재생하는 동안 걸어 둘 사운드 믹스. 레벨 배경음을 끄는 믹스를 넣으면 연출 중에는 BGM이 나오지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Story")
	TObjectPtr<USoundMix> SoundMix;

	// 시네마틱은 NPC 대화창 대신 화면 하단의 자막을 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Story")
	bool bUseSubtitles = false;

	// 마을 첫 전경처럼 반드시 보여줄 장면은 스킵을 허용하지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Story")
	bool bAllowSkip = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Story", meta = (EditCondition = "bUseSubtitles"))
	FSlateFontInfo SubtitleFont;

	// 시퀀스 제작용 기본값으로, 재생 시간은 Level Sequence의 키가 결정한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Story", meta = (ClampMin = "0.1"))
	float SecondsPerLine = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Story", meta = (ClampMin = "0.001"))
	float SecondsPerCharacter = 0.03f;

	// 페이드아웃과 페이드인을 합친 사진 전환 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Story", meta = (ClampMin = "0.0"))
	float PhotoTransitionSeconds = 0.8f;

	// 도트 그림책용. 사진을 화면을 덮는 가장 작은 정수 배율로 그리고, 시퀀스의 PhotoPan 이동을 그림 한 칸 단위로 끊는다.
	// 사진 텍스처는 Nearest 필터여야 하며, 화면보다 조금 큰 그림(여백)이 있어야 이동해도 가장자리가 비지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Story")
	bool bPixelPerfectPhotos = false;

	// 켜면 사진이 바뀔 때 이전 사진이 PhotoTransitionSeconds 동안 새 사진 위로 녹아 사라진다(검은 화면을 거치지 않음).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Story")
	bool bCrossfadePhotos = false;

	// 시퀀스의 FlashOpacity 로 덮는 빛의 색
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Story")
	FLinearColor FlashColor = FLinearColor(1.0f, 0.93f, 0.78f, 1.0f);

	// 레터박스 한 줄의 높이(화면 높이 비율)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Story", meta = (ClampMin = "0.0", ClampMax = "0.3"))
	float LetterboxFraction = 0.11f;

	// 시퀀스의 TitleOpacity로 띄우는 타이틀 이미지(로고). 화면 가운데에 그린다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Story")
	TObjectPtr<UTexture2D> TitleImage;

	// 화면 너비 대비 타이틀 이미지 폭
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Story", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float TitleWidthFraction = 0.42f;

	// 화면 높이 대비 타이틀 중심 높이(0 = 위, 1 = 아래)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Story", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TitleCenterHeight = 0.42f;

	// 켜면 스킵 버튼이나 스킵 키(Esc, 패드 B·메뉴)를 HoldToSkipSeconds 동안 눌러야 넘어간다. 한 번 눌러 실수로 넘기지 않게.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Story", meta = (EditCondition = "bAllowSkip"))
	bool bHoldToSkip = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Story", meta = (EditCondition = "bHoldToSkip", ClampMin = "0.1"))
	float HoldToSkipSeconds = 1.0f;

	// 자막 아래 여백(화면 높이 비율). 0이면 기존 60 단위를 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Story", meta = (EditCondition = "bUseSubtitles", ClampMin = "0.0", ClampMax = "0.5"))
	float SubtitleBottomFraction = 0.0f;
};

