#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CharacterSelect/NUCharacterTypes.h"
#include "NUJobSelectionCardWidget.generated.h"

class UButton;
class UImage;
class UOverlay;
class UTextBlock;
class UTexture2D;

DECLARE_DELEGATE_OneParam(FOnNUJobCardClicked, int32)

// 직업 선택 무대의 한 칸 --- 내 캐릭터(고른 캐릭터의 종족·성별·머리·얼굴)가 그 직업 차림으로 정면을 보고 선다.
// 고르지 않은 칸은 정면 대기 첫 그림을 어둡게 세워 두고, 고른 칸은 정면 대기를 재생한다(머리·얼굴은 바운스 표대로 몸을 따라간다).
// 칸 전체가 버튼이라 마우스로 고르고 패드·키보드 포커스도 받는다.
// UNUJobSelectionWidget::JobCardClass로 지정하면 옛 회색 카드 대신 이 칸을 만든다
UCLASS(Abstract)
class PROJECTM_API UNUJobSelectionCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(int32 InJobId, const FText& InName, UTexture2D* InEmblem, const FLinearColor& InAccent, const FNUCharacterSummary& InAppearance);
	void SetSelected(bool bInSelected);

	int32 GetJobId() const { return JobId; }
	UWidget* GetFocusTarget() const;

	FOnNUJobCardClicked OnCardClicked;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 칸 전체를 덮는 투명 버튼 (클릭·포커스)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> CardButton;

	// 몸·얼굴·머리 플립북이 겹쳐 들어갈 자리 (발 = 아래 가운데)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UOverlay> FigureOverlay;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> EmblemImage;

	// 고른 칸에만 보이는 빛 묶음. 안의 LightBeam·LightGround는 직업 색으로 물든다
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SelectedLight;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> LightBeam;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> LightGround;

	// 그림 1칸을 몇 배로 그릴지 --- 정수여야 도트가 고르다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job|UI")
	float FigurePixelScale = 3.f;

	// 고르지 않은 칸의 그림 색 (곱)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job|UI")
	FLinearColor DimmedFigureTint = FLinearColor(0.42f, 0.44f, 0.5f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job|UI")
	FSlateColor NameColor = FSlateColor(FLinearColor(0.33f, 0.36f, 0.4f, 1.f));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job|UI")
	FSlateColor SelectedNameColor = FSlateColor(FLinearColor(0.87f, 0.68f, 0.29f, 1.f));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job|UI")
	FLinearColor EmblemTint = FLinearColor(0.33f, 0.36f, 0.4f, 0.8f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job|UI")
	FLinearColor SelectedEmblemTint = FLinearColor(0.87f, 0.68f, 0.29f, 1.f);

	// 빛 기둥·발밑 고리의 불투명도 (색은 직업 색)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job|UI")
	float BeamOpacity = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job|UI")
	float GroundOpacity = 0.9f;

private:
	void RebuildFigure();

	UFUNCTION()
	void HandleClicked();

	int32 JobId = 0;
	FNUCharacterSummary Appearance;
	FLinearColor Accent = FLinearColor::White;
	bool bSelected = false;
	bool bFigureBuilt = false;
	bool bFigureSelected = false;
};
