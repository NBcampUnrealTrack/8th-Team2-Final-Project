// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NU_CommonButtonWidget.generated.h"

class UButton;
class UImage;
class UTexture2D;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNUCommonButtonClicked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNUCommonButtonClickedWithTag, int32, Tag);

/*
공용 기본 버튼 부품 (WBP_CommonButton). 루트가 Btn_Common(Button) 하나뿐이라 다른 위젯의
트리에 자식으로 얹으면 UButton 이 아니라 이 UUserWidget 래퍼로 취급된다. 그래서 상위 위젯은
버튼을 UButton* 대신 이 클래스로 BindWidget 하고, OnCommonClicked 를 구독한다.
(WBP_CloseButton / UNU_CloseButtonWidget 과 동일한 패턴)

Txt_Label 은 이 위젯 내부(WBP_CommonButton 트리)에 있어서 바깥 WBP의 이름 기반 프로퍼티
설정(WidgetService.set_property 등)으로는 닿지 않는다. 그래서 라벨 문구는 LabelText 를
통해 이 위젯 자신의 인스턴스 프로퍼티로 노출한다 — 바깥 WBP 디자이너에서 ConfirmButton
같은 자식 인스턴스를 선택하면 Details 패널에 LabelText 가 바로 보이고, 값이 NativePreConstruct
에서 Txt_Label 에 그대로 반영된다.

ButtonMargin 도 같은 이유로 인스턴스 프로퍼티다 — WBP_CommonButton 자체 그래프가
Construct 시점에 Btn_Common 의 WidgetStyle 을 하드코딩된 Margin(0)으로 새로 만들어 덮어쓰므로,
그 그래프가 끝난 뒤(NativeConstruct 의 Super 호출 직후) ButtonMargin 값으로 다시 한 번
Normal/Hovered/Pressed/Disabled 브러시의 Margin 을 덮어써서 최종적으로 이긴다.

ButtonSize/bOverrideButtonSize — 버튼 상황마다(채팅 탭 vs Job 확인 버튼 등) 크기가 달라야 해서
추가했다. WBP_CommonButton 애셋 트리에 SizeBox를 넣는 대신, RebuildWidget()에서 이 위젯을
고정 크기 SBox로 감싸는 방식으로 처리한다 — 애셋 구조를 안 건드리니 이미 이 클래스를 쓰는
모든 곳(채팅 탭/채널, Job 확인 버튼 등)에 안전하다. 꺼져 있으면(기본값) 예전처럼 부모 슬롯이
주는 크기를 그대로 채운다.

ButtonMargin — Btn_Common의 배경은 텍스처가 아니라 머티리얼(M_Button_Hovered 기반 Dynamic
Material Instance)이라, Slate의 FSlateBrush::Margin(9-slice 자동 슬라이싱)은 셰이더가 안
읽어서 시각적으로 전혀 효과가 없다(실측 확인 — Margin을 뭘로 넣어도 렌더 결과가 똑같았음).
그래서 진짜 9-slice는 머티리얼 안(M_Button_Hovered)의 Custom HLSL 노드가 직접 계산한다 ---
ApplyButtonMargin()이 그 노드가 쓰는 머티리얼 파라미터(MarginPx, 고정 상수인 BrushSize)를
채워 넣고, NativeTick()이 실제 렌더 크기(WidgetSizePx)를 매 프레임 갱신한다(bOverrideButtonSize
여부와 무관하게 항상 실제 캐시된 지오메트리 크기를 쓰므로 Fill 배치든 고정 크기든 다 맞는다).
머티리얼이 참조하는 원본 텍스처의 논리적 기준 해상도(32x32, WBP_CommonButton 그래프가 예전에
Slate ImageSize로 하드코딩했던 값과 동일)는 NU_CommonButtonWidget.cpp의 BrushNativeSize 상수로
고정돼 있다 --- 인스턴스 프로퍼티로 노출하면 누군가 실수로 ButtonSize와 같은 값을 넣어서 9-slice
효과를 꺼버릴 수 있어서(실제로 한 번 그랬다) 일부러 EditAnywhere로 안 열어뒀다.

Img_Icon/IconTexture — 아이콘 있는 버튼(스탯 세부 정보 등)용. WBP_CommonButton 트리에 Txt_Label과
나란히 Img_Icon(Image)을 넣어두되 기본 Visibility를 Collapsed로 해서, 텍스트만 쓰는 기존 사용처는
전혀 영향받지 않는다. IconTexture를 비워두면(기본값) SetIconTexture가 계속 Collapsed로 두고,
텍스처를 넣으면 HitTestInvisible로 보여준다(아이콘이 버튼 클릭을 가로채지 않게).

IconSize — 아이콘 크기를 버튼마다 다르게 주고 싶을 때 쓴다. WBP에 SizeBox를 새로 넣는 대신
UImage::SetDesiredSizeOverride()를 그대로 쓴다 --- (0,0)이면(기본값) 텍스처 원본 크기 그대로라
기존 사용처는 영향 없다.

Txt_Shortcut/ShortcutText — 게임 메뉴 항목처럼 라벨 오른쪽에 단축키 힌트를 붙이는 버튼용.
Img_Icon과 같은 패턴: WBP_CommonButton 트리에 Txt_Label과 나란히 넣어두되 기본 Visibility를
Collapsed로 해서, 힌트가 필요 없는 기존 사용처는 전혀 영향받지 않는다.
*/
UCLASS()
class PROJECTM_API UNU_CommonButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "NU|UI")
	FOnNUCommonButtonClicked OnCommonClicked;

	// 목록 항목(탭 인덱스, 채널 값 등)처럼 "어떤 버튼인지" 구분할 값이 필요한 호출부용.
	// OnCommonClicked와 별개로 항상 같이 발화하므로, 필요한 쪽만 구독하면 된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|UI")
	int32 Tag = 0;

	UPROPERTY(BlueprintAssignable, Category = "NU|UI")
	FOnNUCommonButtonClickedWithTag OnCommonClickedWithTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|UI")
	FText LabelText;

	// 채널별 색상처럼 라벨 글자색을 다르게 줘야 하는 경우 사용. 기본값(흰색)을 안 바꾸면
	// 디자인타임 Txt_Label 색 그대로라 기존 사용처는 영향 없다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|UI")
	FLinearColor LabelColor = FLinearColor::White;

	// 상황마다 이 버튼 자체의 픽셀 크기를 다르게 주고 싶을 때 켠다. 꺼져있으면(기본값) 지금까지처럼
	// 부모 슬롯이 주는 크기를 그대로 채운다 --- 기존 사용처(채팅 탭 등)는 전혀 영향받지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|UI")
	bool bOverrideButtonSize = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|UI", meta = (EditCondition = "bOverrideButtonSize"))
	FVector2D ButtonSize = FVector2D(101.f, 92.f);

	// 9-slice 테두리 두께 --- 픽셀 단위. 머티리얼의 Custom HLSL 노드가 고정된 기준 해상도(.cpp의
	// BrushNativeSize) 대비 비율로 환산해서 UV를 재계산한다 (ButtonSize와는 무관 --- 버튼이 크든
	// 작든 테두리 두께가 항상 같게 나온다).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|UI")
	FMargin ButtonMargin = FMargin(0.f);

	// 런타임에 라벨을 바꿔야 하는 경우(추적/고정 해제처럼 상태에 따라 문구가 바뀌는 버튼) 사용.
	// LabelText도 같이 갱신하므로 이후 NativePreConstruct가 다시 실행돼도 값이 유지된다.
	UFUNCTION(BlueprintCallable, Category = "NU|UI")
	void SetLabelText(const FText& InText);

	// 런타임에 라벨 색을 바꿔야 하는 경우(채팅 채널별 색상 등) 사용. SetLabelText와 동일한 패턴.
	UFUNCTION(BlueprintCallable, Category = "NU|UI")
	void SetLabelColor(const FLinearColor& InColor);

	// 버튼마다 라벨 크기를 다르게 줘야 할 때 사용(작은 화살표 버튼 vs 큰 확인 버튼 등). 0이면(기본값)
	// WBP_CommonButton 자체에 디자인타임으로 설정된 폰트 크기를 그대로 쓴다 --- 기존 사용처는 안 바뀐다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|UI", meta = (ClampMin = 0))
	int32 LabelFontSize = 0;

	// 런타임에 라벨 크기를 바꿔야 하는 경우 사용. SetLabelText와 동일한 패턴. 0을 넣으면 다시
	// WBP 디자인타임 크기로 돌아간다는 뜻은 아니고(이미 덮어쓴 폰트는 그대로 남는다) 그냥 무시된다.
	UFUNCTION(BlueprintCallable, Category = "NU|UI")
	void SetLabelFontSize(int32 InSize);

	// 아이콘 있는 버튼(스탯 세부 정보 등)에서 사용. 비워두면(기본값) 아이콘 자리가 Collapsed라
	// 기존 텍스트 전용 버튼은 전혀 영향받지 않는다 --- Img_Icon이 WBP에 없어도 안전하게 무시된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|UI")
	TObjectPtr<UTexture2D> IconTexture;

	// 런타임에 아이콘을 바꿔야 하는 경우 사용. SetLabelText와 동일한 패턴 --- IconTexture도 갱신되므로
	// 이후 NativePreConstruct가 다시 실행돼도 값이 유지된다. nullptr을 넣으면 아이콘을 다시 감춘다.
	UFUNCTION(BlueprintCallable, Category = "NU|UI")
	void SetIconTexture(UTexture2D* InTexture);

	// 아이콘 픽셀 크기 --- (0,0)이면(기본값) 텍스처 원본 크기를 그대로 쓴다. 버튼마다 아이콘을
	// 다르게 키우거나 줄이고 싶을 때(작은 화살표 버튼 vs 큰 스탯 버튼 등) 여기 지정한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|UI")
	FVector2D IconSize = FVector2D::ZeroVector;

	// 런타임에 아이콘 크기를 바꿔야 하는 경우 사용. SetLabelText와 동일한 패턴.
	UFUNCTION(BlueprintCallable, Category = "NU|UI")
	void SetIconSize(const FVector2D& InSize);

	// 아이콘 텍스처 하나가 버튼 그림 전체인 경우(채팅 헤더의 화살표/추가 버튼 등) 배경 9-slice
	// pill을 감추고 싶을 때 켠다. 꺼져 있으면(기본값) 지금까지처럼 배경 머티리얼이 그대로 보인다 ---
	// 기존 사용처(라벨 버튼 등)는 전혀 영향받지 않는다. ButtonMargin과 같은 이유로, WBP_CommonButton
	// 그래프가 Btn_Common의 WidgetStyle을 새로 만든 뒤(NativePreConstruct/NativeConstruct의 Super
	// 호출 직후) 이 값이 켜져 있으면 Normal/Hovered/Pressed 브러시를 DrawAs=NoDrawType으로 다시
	// 덮어써서 배경을 지운다 --- SetBackgroundColor처럼 SlateColor 틴트로는 안 먹힌다(머티리얼이
	// VertexColor를 안 읽음, ApplyButtonMargin 주석 참고). DrawAs=NoDrawType은 머티리얼과 무관하게
	// 항상 먹히므로 이 방법을 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|UI")
	bool bIconOnly = false;

	// 게임 메뉴 항목처럼 라벨 오른쪽에 단축키 힌트("Esc" 등)를 붙이고 싶은 버튼에서 사용.
	// 비워두면(기본값) Collapsed라 기존 사용처는 전혀 영향받지 않는다 --- IconTexture와 동일한 패턴.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|UI")
	FText ShortcutText;

	// 런타임에 단축키 힌트를 바꿔야 하는 경우 사용. SetLabelText와 동일한 패턴. 빈 텍스트를 넣으면
	// 다시 Collapsed로 감춘다.
	UFUNCTION(BlueprintCallable, Category = "NU|UI")
	void SetShortcutText(const FText& InText);

	// 버튼 배경색 틴트 --- 순정 UButton::SetBackgroundColor를 그대로 이 위젯에도 노출한다.
	// 주의: Btn_Common의 배경이 텍스처가 아니라 머티리얼(M_Button_Hovered)이고, 그 머티리얼이
	// VertexColor를 아예 안 읽기 때문에(ButtonMargin과 같은 이유) 이 함수는 지금 시각적으로
	// 아무 효과가 없다 --- 나중에 머티리얼이 VertexColor를 쓰게 바뀌면 그때 유효해진다.
	// 선택 상태 표시는 대신 SetSelected()(텍스처 자체를 바꿈)를 쓴다.
	UFUNCTION(BlueprintCallable, Category = "NU|UI")
	void SetBackgroundColor(const FLinearColor& InColor);

	// 탭처럼 "선택됨" 상태를 다른 텍스처로 보여주고 싶은 버튼에서 사용. 실제 텍스처 교체는
	// WBP_CommonButton 그래프가 담당한다 --- 이미 만들어져 있는 DynMat의 ButtonTexture 파라미터만
	// NormalButtonTexture/SelectedButtonTexture 사이로 바꿔치우므로 MID를 다시 만들 필요가 없다.
	// 그 두 텍스처를 안 채운 기존 버튼(확인창 등)은 그래프가 호출돼도 조용히 아무 효과가 없다.
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|UI")
	void SetSelected(bool bInSelected);

protected:
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;

	// 실제 렌더 크기(WidgetSizePx)를 머티리얼에 매 프레임 반영 --- Fill 배치라 크기가 레이아웃마다
	// 바뀔 수 있는 버튼(채팅 탭 등)도 항상 정확한 9-slice 비율을 유지하게 하려면 이 방법뿐이다.
	// 크기가 실제로 안 바뀌었으면(대부분의 프레임) 파라미터를 다시 안 써서 비용은 무시할 만하다.
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// bOverrideButtonSize가 켜져 있으면 이 위젯을 고정 크기 SBox로 감싼다. WBP_CommonButton
	// 애셋 구조(루트에 SizeBox 추가 등)는 건드리지 않고 C++만으로 크기를 강제하는 방법이다.
	virtual TSharedRef<SWidget> RebuildWidget() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> Btn_Common;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Label;

	// 아이콘 있는 버튼에서만 쓴다 --- WBP에 없으면 IsValid 검사로 안전하게 건너뛴다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Icon;

	// 단축키 힌트 있는 버튼(게임 메뉴 등)에서만 쓴다 --- WBP에 없으면 IsValid 검사로 안전하게 건너뛴다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Shortcut;

private:
	// MarginPx/BrushSize 머티리얼 파라미터를 채운다 (렌더 크기와 무관한 부분).
	void ApplyButtonMargin();

	// bIconOnly가 켜져 있으면 Btn_Common의 배경 브러시를 DrawAs=NoDrawType으로 덮어써서 숨긴다.
	void ApplyIconOnlyMode();

	// Btn_Common의 현재 브러시 리소스(Dynamic Material Instance)를 가져온다. 없으면 nullptr.
	class UMaterialInstanceDynamic* GetBrushMaterial() const;

	// WidgetSizePx 머티리얼 파라미터를 채운다. NativeTick에서 크기가 바뀌었을 때만 호출된다.
	void ApplyWidgetSizeToMaterial(const FVector2D& SizePx);

	// 마지막으로 머티리얼에 반영한 렌더 크기 --- 매 프레임 불필요하게 다시 쓰지 않기 위한 캐시.
	FVector2D LastAppliedWidgetSize = FVector2D::ZeroVector;

	UFUNCTION()
	void HandleClicked();
};
