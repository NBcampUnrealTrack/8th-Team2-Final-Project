#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NUStarterNaiaCardWidget.generated.h"

class UButton;
class UImage;
class UNUFlipbookImage;
class UPaperFlipbook;
class UTextBlock;
class UTexture2D;

DECLARE_DELEGATE_OneParam(FOnNUStarterNaiaCardClicked, int32)

// 시작 나이아 무대의 한 칸 --- 나이아 대기 플립북, 속성 그림 아이콘, 이름. 고른 칸은 크게(SelectedPixelScale) 앞으로 나와
// 속성 색 빛을 받고, 나머지는 어둡게 물러난다. 칸 전체가 버튼이다.
// UNUStarterNaiaSelectionWidget::NaiaCardClass로 지정하면 옛 회색 카드 대신 이 칸을 만든다
UCLASS(Abstract)
class PROJECTM_API UNUStarterNaiaCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(int32 InNaiaId, const FText& InName, UPaperFlipbook* InFlipbook, UTexture2D* InElementIcon, const FLinearColor& InElementColor);
	void SetSelected(bool bInSelected);

	int32 GetNaiaId() const { return NaiaId; }
	UWidget* GetFocusTarget() const;

	FOnNUStarterNaiaCardClicked OnCardClicked;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> CardButton;

	// 나이아 그림 (발 = 아래 가운데에 두고 정렬)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UNUFlipbookImage> FigureImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> ElementIcon;

	// 고른 칸에만 보이는 빛 묶음. 안의 LightBeam·LightGround는 속성 색으로 물든다
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SelectedLight;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> LightBeam;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> LightGround;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|UI")
	float FigurePixelScale = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|UI")
	float SelectedPixelScale = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|UI")
	FLinearColor DimmedFigureTint = FLinearColor(0.42f, 0.44f, 0.5f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|UI")
	FSlateColor NameColor = FSlateColor(FLinearColor(0.33f, 0.36f, 0.4f, 1.f));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|UI")
	FSlateColor SelectedNameColor = FSlateColor(FLinearColor(0.87f, 0.68f, 0.29f, 1.f));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|UI")
	float BeamOpacity = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|UI")
	float GroundOpacity = 0.9f;

private:
	UFUNCTION()
	void HandleClicked();

	int32 NaiaId = 0;
	bool bSelected = false;

	UPROPERTY(Transient)
	TObjectPtr<UPaperFlipbook> Flipbook;
};
