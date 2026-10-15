// 옵션 창 --- ESC 전체 메뉴의 [옵션]이 연다.
// 화면/그래픽은 엔진 UGameUserSettings를 그대로 읽고 쓴다 (적용, 저장, 되돌리기 전부 엔진 것이라 저장 코드가 없다).
// 사운드는 UNUGameUserSettings(엔진 설정 파생)의 볼륨을 읽고 쓴다 --- 슬라이더를 움직이면 바로 들리고, 저장은 [적용].
// 키 설정은 시스템이 아직 없어 WBP에 자리만 잡아 두었다.
//
// 팀 패턴: C++는 로직/계약만, 모양은 WBP(WBP_OptionsWindow)가 담당한다.
// 아래 위젯은 BindWidget이라 WBP에 같은 이름·타입이 없으면 WBP 컴파일이 실패한다 --- 이름이 바뀌어도 조용히 죽지 않는다:
//   탭:           TabSwitcher(WidgetSwitcher) + 같은 순서의 DisplayTabButton / GraphicsTabButton / SoundTabButton / ControlsTabButton
//   화면:         WindowModeCombo / ResolutionCombo / VSyncCheck / FrameLimitCombo  (ComboBoxString / CheckBox)
//   그래픽:       라디오 묶음(UNUOptionRadioGroupWidget, WBP_OptionRadioGroup) 9줄 --- 콤보가 아니라 네모 버튼 4개
//                 QualityGroup(프리셋 하/중/상/최상/사용자 설정 --- 세부가 서로 다르면 "사용자 설정"이 켜진다)
//                 + 세부 7개 ShadowQualityGroup / GlobalIlluminationQualityGroup / ReflectionQualityGroup / PostProcessQualityGroup
//                   / EffectsQualityGroup / TextureQualityGroup / ViewDistanceQualityGroup  (프리셋을 고르면 한 번에 채워지고,
//                   하나라도 손대면 프리셋 표시를 다시 계산한다. 적용은 항상 세부 7개 값으로)
//                 + UpscaleGroup(끔/품질/균형/성능 --- UNUGameUserSettings::UpscaleMode, 켜면 TSR)
//   사운드:       MasterVolumeSlider / BgmVolumeSlider / SfxVolumeSlider  (Slider) + MasterVolumeValueText / BgmVolumeValueText / SfxVolumeValueText (숫자)
//   조작:         KeyBindingsButton  (키 설정 창을 연다 --- 창은 UNUKeyBindingsWindow)
//   하단:         DefaultsButton / ApplyButton / CloseButton
// 콤보 항목은 C++가 채운다 (순서가 엔진 열거형 값과 1:1이라 WBP가 손대면 안 된다)
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/GameUserSettings.h" // 세부 품질 줄이 엔진 설정의 멤버 함수 포인터를 든다 --- 전방 선언으로는 MSVC가 포인터 표현을 못 정한다
#include "NUOptionsWindowWidget.generated.h"

class UButton;
class UCheckBox;
class UComboBoxString;
class UNUConfirmDialog;
class UNUOptionRadioGroupWidget;
class USlider;
class UTextBlock;
class UWidget;
class UWidgetSwitcher;

UCLASS(Abstract)
class PROJECTM_API UNUOptionsWindowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ESC 메뉴가 부른다. 클래스는 프로젝트 세팅(NU UI)의 OptionsWindowClass --- 미지정이면 로그만 남긴다
	static UNUOptionsWindowWidget* Show(APlayerController* OwningPlayer);

	// UI 값을 엔진 설정에 쓰고 적용/저장한다. 창 모드나 해상도가 바뀌었으면 "유지할까요?" 확인을 거친다
	UFUNCTION(BlueprintCallable, Category = "NU|UI")
	void Apply();

	// 엔진 기본값을 UI에 채운다 (적용 전까지는 화면에 반영되지 않는다)
	UFUNCTION(BlueprintCallable, Category = "NU|UI")
	void ResetToDefaults();

	// 적용하지 않은 변경은 버린다
	UFUNCTION(BlueprintCallable, Category = "NU|UI")
	void Close();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	// 엔진 설정의 현재 값으로 콤보/체크를 채운다
	void LoadFromSettings();

	// 해상도 변경 뒤 확인 --- 시간 안에 답이 없으면 되돌린다 (엔진의 LastConfirmed 값으로)
	void BeginVideoModeConfirm();
	void TickVideoModeCountdown();
	void FinishVideoModeConfirm(bool bKeep);
	void HandleVideoModeKept() { FinishVideoModeConfirm(true); }
	void HandleVideoModeReverted() { FinishVideoModeConfirm(false); }
	FText GetVideoModeCountdownText() const;

	// 콤보 도우미 --- 항목을 채우고 고른 번호를 읽는다
	static void FillCombo(UComboBoxString* Combo, const TArray<FString>& Options, int32 SelectedIndex);
	static int32 GetComboIndex(const UComboBoxString* Combo);

	// 세부 품질 한 줄 = 라디오 묶음 + 그 항목을 읽고 쓰는 엔진 설정 함수. 순서는 WBP 줄 순서와 무관
	struct FQualityRow
	{
		UNUOptionRadioGroupWidget* Group;
		int32 (UGameUserSettings::*Get)() const;
		void (UGameUserSettings::*Set)(int32);
	};
	TArray<FQualityRow, TInlineAllocator<7>> GetQualityRows() const;

	// 세부 7개가 전부 같은 단계면 그 단계, 아니면 "사용자 설정"(= 단계 수, 프리셋 묶음의 5번째 버튼)
	int32 ComputePresetIndex() const;

	// 프리셋을 누르면 세부 7개를 그 단계로 ("사용자 설정"은 세부를 그대로 둔다). 세부를 누르면 프리셋 표시를 다시 계산한다
	void HandlePresetSelected(int32 Preset);
	void HandleCategorySelected(int32 Index);

	// 사운드 슬라이더 --- 움직이면 바로 들리게 반영한다 (저장은 [적용], 그 전에 창이 사라지면 되돌린다)
	UFUNCTION() void HandleMasterVolumeChanged(float Value);
	UFUNCTION() void HandleBgmVolumeChanged(float Value);
	UFUNCTION() void HandleSfxVolumeChanged(float Value);
	void PreviewVolume(UTextBlock* ValueText, float Value);

	// 슬라이더와 옆 숫자를 값에 맞춘다
	static void SetVolumeRow(USlider* Slider, UTextBlock* ValueText, float Value);
	static void SetVolumeText(UTextBlock* ValueText, float Value);

	// 해상도 목록은 창 모드에 따라 다르다 --- 전체 화면은 모니터가 지원하는 모드만, 테두리 없는 창은 바탕화면 하나(콤보 비활성),
	// 창 모드는 권장 창 크기. 목록에 없는 값을 못 고르게 해서 "작은 전체 화면" 같은 조합을 막는다.
	// Preferred: 목록에 있으면 그것을 고른다. 없으면 가장 큰 것(= 바탕화면/네이티브)
	void RefreshResolutionList(int32 WindowMode, FIntPoint Preferred);

	// 창 모드를 바꾸면 해상도 목록을 갈아 끼운다. 전체 화면 계열로 바꾸면 바탕화면 해상도를 기본으로 잡는다
	UFUNCTION()
	void HandleWindowModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	// 탭 --- 페이지 전환은 C++, 선택 표시는 상점 창과 같은 방식(배경색)
	void SelectTab(int32 Index);
	void UpdateTabVisual();

	// 명명 계약: 순정 Button(레거시)과 UNU_CommonButtonWidget(신규 공용 부품) 둘 다 지원해야 해서
	// BindWidget 타입을 UWidget으로 열어두고 여기서 캐스팅해 알맞은 델리게이트/함수로 다룬다
	// (NUQuestWidget 등과 동일 패턴). Bind는 FScriptDelegate로 양쪽 OnClicked/OnCommonClicked에 다 걸 수 있다
	static void BindButtonClicked(UWidget* Widget, UObject* Target, FName FunctionName);
	static void UnbindButtonClicked(UWidget* Widget, UObject* Target, FName FunctionName);
	static void SetButtonBackgroundColor(UWidget* Widget, const FLinearColor& Color);

	// UButton::OnClicked는 페이로드가 없어 탭마다 하나씩 (상점 창 Buy/Sell 탭과 같은 처리)
	UFUNCTION() void HandleDisplayTabClicked() { SelectTab(0); }
	UFUNCTION() void HandleGraphicsTabClicked() { SelectTab(1); }
	UFUNCTION() void HandleSoundTabClicked() { SelectTab(2); }
	UFUNCTION() void HandleControlsTabClicked() { SelectTab(3); }

	UFUNCTION()
	void HandleKeyBindingsClicked();

	int32 ActiveTab = 0;

	// ResolutionCombo 항목 순서와 1:1
	TArray<FIntPoint> Resolutions;

	FTimerHandle VideoModeConfirmTimer;
	int32 VideoModeSecondsLeft = 0;
	TWeakObjectPtr<UNUConfirmDialog> VideoModeDialog;

	// [기본값]이나 볼륨 슬라이더로 엔진 설정 객체를 바꿨지만 아직 [적용]하지 않음 --- 창이 사라질 때 디스크 값으로 되돌린다
	bool bSettingsTouched = false;

	// 아래는 전부 WBP_OptionsWindow의 위젯 (BindWidget). 콤보 항목만 C++가 채운다
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UWidgetSwitcher> TabSwitcher;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UWidget> DisplayTabButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UWidget> GraphicsTabButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UWidget> SoundTabButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UWidget> ControlsTabButton;

	// 조작 탭의 [키 설정] --- 별도 창을 연다 (탭 안에 목록을 넣으면 동작 수에 따라 탭 높이가 흔들린다)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UWidget> KeyBindingsButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UComboBoxString> WindowModeCombo;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UComboBoxString> ResolutionCombo;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UCheckBox> VSyncCheck;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UComboBoxString> FrameLimitCombo;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UNUOptionRadioGroupWidget> QualityGroup;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UNUOptionRadioGroupWidget> ShadowQualityGroup;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UNUOptionRadioGroupWidget> GlobalIlluminationQualityGroup;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UNUOptionRadioGroupWidget> ReflectionQualityGroup;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UNUOptionRadioGroupWidget> PostProcessQualityGroup;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UNUOptionRadioGroupWidget> EffectsQualityGroup;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UNUOptionRadioGroupWidget> TextureQualityGroup;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UNUOptionRadioGroupWidget> ViewDistanceQualityGroup;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UNUOptionRadioGroupWidget> UpscaleGroup;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class USlider> MasterVolumeSlider;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UTextBlock> MasterVolumeValueText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class USlider> BgmVolumeSlider;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UTextBlock> BgmVolumeValueText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class USlider> SfxVolumeSlider;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UTextBlock> SfxVolumeValueText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UWidget> DefaultsButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UWidget> ApplyButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UWidget> CloseButton;
};
