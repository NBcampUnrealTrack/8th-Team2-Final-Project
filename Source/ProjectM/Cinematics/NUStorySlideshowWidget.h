#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Common/NUTextReveal.h"
#include "NUStorySlideshowWidget.generated.h"

class UNUStorySlideshowData;
class ANUStorySequenceCue;
class UBorder;
class UButton;
class UCanvasPanel;
class UTexture2D;
class UImage;
class UProgressBar;
class UScaleBox;
class UTextBlock;
class UWidget;

// 사진과 기존 대사창을 표시한다. 재생 시간과 입력 모드는 호출자가 관리한다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNUStorySlideshowFinished);

UCLASS()
class PROJECTM_API UNUStorySlideshowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	bool StartSequencePresentation(UNUStorySlideshowData* InData, ANUStorySequenceCue* InCue);

	UFUNCTION(BlueprintCallable, Category = "NU|Story")
	void FinishSlideshow();

	UPROPERTY(BlueprintAssignable, Category = "NU|Story")
	FNUStorySlideshowFinished OnFinished;

	// 사용자 스킵과 외부 종료를 구분한다.
	UPROPERTY(BlueprintAssignable, Category = "NU|Story")
	FNUStorySlideshowFinished OnSkipRequested;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Story")
	int32 SlideIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Story")
	int32 LineIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Story")
	bool bIsPlaying = false;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "NU|Story")
	TObjectPtr<UUserWidget> DialogueWindow;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnFocusLost(const FFocusEvent& InFocusEvent) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UNUStorySlideshowData> Data;

	UPROPERTY(Transient)
	TObjectPtr<ANUStorySequenceCue> SequenceCue;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Backdrop;

	UPROPERTY(Transient)
	TObjectPtr<UImage> Picture;

	UPROPERTY(Transient)
	TObjectPtr<UScaleBox> PictureScale;

	UPROPERTY(Transient)
	TObjectPtr<UImage> Title;

	// 크로스페이드용 이전 사진. 새 사진 위에 얹혀 녹아 사라진다.
	UPROPERTY(Transient)
	TObjectPtr<UScaleBox> PrevPictureScale;

	UPROPERTY(Transient)
	TObjectPtr<UImage> PrevPicture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> PrevTexture;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> MoteLayer;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> MoteBoxes;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Flash;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> LetterboxTop;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> LetterboxBottom;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Dialogue;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Subtitle;

	UPROPERTY(Transient)
	TObjectPtr<UButton> SkipButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SkipLabel;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> SkipProgress;

	// 길게 눌러 건너뛰기 모양(프롤로그): 키 칩 + 자막과 같은 글꼴 + 아래 금빛 진행 줄. 기존 짧게 누르는 스킵 버튼 모양은 그대로 둔다.
	UPROPERTY(Transient)
	TObjectPtr<UWidget> HoldSkipContent;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> SkipKeyRim;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> SkipKeyFill;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SkipKeyLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HoldSkipLabel;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> HoldSkipBar;

	void BuildHoldSkipContent();
	void ApplyHoldSkipStyle();
	void UpdateHoldSkipLook(float DeltaSeconds);
	bool IsHoldSkipStyle() const;

	UFUNCTION()
	void RequestSkip();

	// 길게 눌러 스킵: 버튼과 키가 각각 누르고 있는지 따로 센다.
	UFUNCTION()
	void HandleSkipPressed();

	UFUNCTION()
	void HandleSkipReleased();

	static bool IsSkipKey(const FKey& Key);
	void UpdateHoldToSkip(float DeltaSeconds);
	void ApplyPhotoLayout(const FGeometry& MyGeometry);
	void LayoutPhoto(UScaleBox* Box, const UTexture2D* Texture, float PanX, float PanY, const FGeometry& MyGeometry) const;
	int32 GetPixelsPerTexel(const UTexture2D* Texture, const FGeometry& MyGeometry) const;
	void UpdateCrossfade(float DeltaSeconds, const FGeometry& MyGeometry);
	void UpdateOverlays(const FGeometry& MyGeometry);
	void UpdateMotes(float DeltaSeconds, const FGeometry& MyGeometry);

	// 도트 빛 입자 하나. 위치는 화면 비율, 그릴 때 그림 한 칸 격자에 맞춘다.
	struct FMote
	{
		FVector2D Position = FVector2D::ZeroVector;
		float Speed = 0.0f;
		float Sway = 0.0f;
		float Phase = 0.0f;
		float Age = 0.0f;
		float Life = 1.0f;
		int32 SizeTexels = 1;
	};
	void ResetMote(int32 Index, bool bAnywhere);
	TArray<FMote> Motes;
	float PrevFade = 0.0f;
	float PrevPanX = 0.0f;
	float PrevPanY = 0.0f;
	float LastPanX = 0.0f;
	float LastPanY = 0.0f;
	float LastPhotoOpacity = 0.0f;
	void ApplyTitleLayout(const FGeometry& MyGeometry);
	bool bSkipButtonHeld = false;
	bool bSkipKeyHeld = false;
	float SkipHoldSeconds = 0.0f;
	// 건너뛰기 표시가 나타난 뒤 흐른 시간, 강조(가리키거나 누르는 중) 정도, 마지막 입력이 패드였는지
	float SkipShownSeconds = 0.0f;
	float SkipEmphasis = 0.0f;
	bool bSkipUsingGamepad = false;

	void PrepareDialogueWindow();
	FNUTextReveal TextReveal;
	int32 DisplayedSlideIndex = INDEX_NONE;
};

