#pragma once

#include "CoreMinimal.h"
#include "Components/Image.h"
#include "NUFlipbookImage.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPaperFlipbook;

// UMG에서 플립북을 재생하는 이미지 --- Slate 브러시는 플립북을 직접 못 그리므로
// 타이머로 키프레임 스프라이트를 갈아끼운다. 캐릭터 선택창 미리보기용 (C++가 동적 생성하는 임시 UI 공용)
UCLASS()
class PROJECTM_API UNUFlipbookImage : public UImage
{
	GENERATED_BODY()

public:
	// InDesiredHeight에 맞춰 원본 비율로 스케일해 재생한다. nullptr이면 재생 중지 + 브러시 비움
	void SetFlipbook(UPaperFlipbook* InFlipbook, float InDesiredHeight);

	// 높이에 맞추는 대신 그림 1칸을 InPixelScale배로 그린다 --- 정수 배율이면 도트가 고르게 나온다 (0이면 높이 맞춤으로 되돌린다)
	void SetFlipbookAtScale(UPaperFlipbook* InFlipbook, float InPixelScale);

	// 머리 색 --- 그림의 밝기만 남기고 InTint로 물들이는 UI 머티리얼로 그린다 (파라미터 SpriteTexture · UVRect · HairTint).
	// nullptr이면 원래 그림 그대로
	void SetTintMaterial(UMaterialInterface* InMaterial, const FLinearColor& InTint);

	// 머리·얼굴 층이 몸 그림의 키프레임별 위아래 흔들림을 따라가게 한다 --- 필드의 레이어 바운스와 같은 표(NUSpriteFrameOffset).
	// 몸과 같은 틱에 재생을 시작한 층에 쓴다 (키프레임 번호가 몸과 같다). nullptr이면 따라가지 않는다.
	// ExtraUpTexels는 표와 상관없이 늘 더 올리는 양 (그림 칸, 음수면 내림 --- FNUBodyRow::PortraitPartTuneZ)
	void FollowBodyBounce(const UPaperFlipbook* BodyFlipbook, float ExtraUpTexels = 0.f);

	// 재생을 멈추고 KeyFrameIndex번 그림에 둔다 (직업 선택의 고르지 않은 칸 --- 첫 그림으로 세워 둔다)
	void StopAtKeyFrame(int32 KeyFrameIndex);

	UPaperFlipbook* GetFlipbook() const { return Flipbook; }

protected:
	virtual void BeginDestroy() override;

private:
	// KeyFrameIndex번 키프레임 스프라이트를 원본 비율로 그린다
	void ShowFrame(int32 KeyFrameIndex);

	// 지금 시각의 키프레임을 그리고, 그 키프레임이 끝나는 때에 다시 불리게 타이머를 건다
	void HandleFrameTimer();

	UPROPERTY(Transient)
	TObjectPtr<UPaperFlipbook> Flipbook;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> TintMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TintInstance;

	FLinearColor Tint = FLinearColor::White;
	// 따라갈 몸 그림의 키프레임별 위로 올라간 양(스프라이트 유닛) --- 측정 캐시의 복사본
	TArray<float> BounceOffsets;
	float BounceExtraUpTexels = 0.f;
	int32 ShownKeyFrame = 0;

	float DesiredHeight = 140.f;
	float PixelScale = 0.f;
	// 재생을 시작한 월드 시각 --- 키프레임마다 길이(FrameRun)가 달라 프레임 번호로 세지 않고 흐른 시간으로 찾는다
	double StartTime = 0.0;
	FTimerHandle FrameTimer;
};
