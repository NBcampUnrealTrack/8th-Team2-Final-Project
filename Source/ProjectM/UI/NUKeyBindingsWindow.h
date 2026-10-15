// 키 설정 창 --- 옵션창 조작 탭의 [키 설정]이 연다.
// 키 저장·불러오기·기본값 복원은 엔진(UEnhancedInputUserSettings)이 하고, 이 창은 목록과 입력 대기, 적용 전 검사만 맡는다.
//
// 줄마다 키보드 칸과 패드 칸이 있다 (9/15 패드). 엔진 프로필에서 한 동작(행)에 매핑이 여러 개일 수 있는데 슬롯 번호 대신 키 종류로 가른다 ---
//   키보드 칸 = 기본 키(IMC)가 키보드 키인 매핑, 패드 칸 = 기본 키가 패드 키이거나 없는 매핑. 패드 기본 키가 없는 동작에 패드 키를 주면
//   빈 슬롯을 새로 만든다 --- 그 키는 엔진이 못 붙이므로 UNUKeyProfile(Core/NUInputUserSettings)이 리빌드 때 키보드 매핑 줄에 얹는다.
//   IMC 등록(행 만들기)은 UNUInputUserSettings가 시작 때 한다.
// 흐름: [키보드]/[패드] => 그 장치의 키 입력 대기 => 누른 키를 임시로 잡아둔다(아직 반영 안 됨) => [적용]에서 한 번에 반영.
//   대기 중엔 다른 장치의 입력은 무시한다 (패드 칸을 바꾸는데 마우스를 눌러도 안 잡히게). 취소는 ESC 또는 패드 B --- 그래서 둘은 지정할 수 없다.
//   패드 모드: LB = 목록 맨 위, RB = [적용]으로 건너뛴다. 지정·취소 뒤엔 그 줄의 바꾸기 버튼에 포커스를 되돌린다 (줄 위젯이 새로 만들어지므로).
// 같은 장치에서 같은 키를 이미 쓰는 동작이 있으면 그 동작에서 떼어낸다. 떼여서 키가 빈 동작이 남은 채 [적용]을 누르면
//   - 꼭 필요한 동작(NUUISettings의 bRequired)이면 안내만 하고 적용하지 않는다 (키보드 칸 기준 --- 패드 칸은 비어 있어도 된다)
//   - 없어도 되는 동작이면 "지정 안 된 채로 적용할까요?"를 묻는다
//
// BindWidget: WBP에 같은 이름·타입의 위젯이 없으면 WBP 컴파일이 실패한다. 선택은 WBP가 빼도 된다.
//   RowBox        (PanelWidget)          줄(WBP_KeyBindingRow)과 묶음 제목이 채워진다
//   ApplyButton   (WBP_CommonButton)     [적용]
//   DefaultsButton(WBP_CommonButton)     [기본값으로]
//   CloseButton   (아무 위젯, 선택)      베이스가 아니라 이 창이 직접 묶는다 (순정 Button / 공용 닫기·일반 버튼)
//   GuideText     (TextBlock, 선택)      "바꿀 항목의 [키보드]나 [패드]를 누르세요" 같은 안내. 대기 중에는 문구가 바뀐다
//   RowWidgetClass / GroupHeaderClass (EditDefaultsOnly)  줄·묶음 제목 WBP --- 창 WBP 기본값에서 지정한다
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NUKeyBindingsWindow.generated.h"

class UNUKeyBindingRowWidget;
class UNU_CommonButtonWidget;

namespace NUKeyBindings
{
	// 플레이어가 쓰는 실제 키 (엔진 사용자 설정 기준, 없으면 빈 키).
	// bGamepad = 패드 칸(패드 키인 매핑), 아니면 키보드·마우스 칸. 키 설정 창과 메뉴 단축키 표기가 같은 값을 보게 하려고 밖으로 뺐다
	PROJECTM_API FKey GetMappedKey(const APlayerController* OwningPlayer, FName MappingName, bool bGamepad);

	// 표기용 글자 --- 지금 쓰는 장치 기준 (패드 모드면 패드 칸, 아니면 키보드 칸). 키가 없으면 빈 글자 (표기 자리를 비우는 쪽이 "지정 안 됨"보다 읽기 좋다)
	PROJECTM_API FText GetMappedKeyText(const APlayerController* OwningPlayer, FName MappingName);

	// 키 하나의 짧은 표기. 패드 키는 Xbox 이름(A/B/X/Y, LB/RB, LT/RT, L3/R3, Start/Back, D패드 방향, 스틱), 그 외는 엔진 표시 이름.
	// 엔진 표시 이름은 "Gamepad Face Button Bottom"처럼 길어서 메뉴 옆에 못 붙인다
	PROJECTM_API FText GetKeyText(const FKey& Key);
}
class UPanelWidget;
class UTextBlock;
class UEnhancedInputUserSettings;

UCLASS(Abstract)
class PROJECTM_API UNUKeyBindingsWindow : public UUserWidget
{
	GENERATED_BODY()

public:
	// 옵션창이 부른다. 클래스는 프로젝트 세팅(NU UI)의 KeyBindingsWindowClass --- 미지정이면 로그만 남기고 nullptr
	static UNUKeyBindingsWindow* Show(APlayerController* OwningPlayer);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 키 입력 대기 중에는 그 장치의 키를 받아 그 줄에 넣는다. ESC·패드 B는 취소, 다른 장치의 키는 무시
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// 대기 중에 스틱을 밀어도 포커스가 다른 줄로 가지 않게 삼킨다 (스틱은 지정 대상이 아니다)
	virtual FReply NativeOnAnalogValueChanged(const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogEvent) override;

private:
	UFUNCTION() void HandleApplyClicked();
	UFUNCTION() void HandleDefaultsClicked();
	UFUNCTION() void HandleCloseClicked();

	// 줄의 [키보드]/[패드] --- 이 동작의 그 장치 키 입력을 기다린다
	void BeginCapture(FName MappingName, bool bGamepad);
	void EndCapture();

	// 누른 키를 임시 변경에 넣는다. 같은 장치에서 같은 키를 쓰던 동작은 비운다
	void ApplyCapturedKey(const FKey& NewKey);

	void Rebuild();
	void CloseSelf();

	// 패드 모드에서 목록을 다시 그린 뒤 그 줄의 바꾸기 버튼에 포커스를 되돌린다 (줄 위젯이 새로 만들어져 포커스가 사라진다)
	void RestoreRowFocus(FName MappingName, bool bGamepad);

	// 지금 화면에 보여야 할 키 (임시 변경이 있으면 그 값, 없으면 엔진에 저장된 값)
	FKey GetEffectiveKey(FName MappingName, bool bGamepad) const;
	FKey GetSavedKey(FName MappingName, bool bGamepad) const;

	// 장치별 임시 변경 표
	TMap<FName, FKey>& GetPendingMap(bool bGamepad) { return bGamepad ? PendingPadKeys : PendingKeys; }
	const TMap<FName, FKey>& GetPendingMap(bool bGamepad) const { return bGamepad ? PendingPadKeys : PendingKeys; }

	UEnhancedInputUserSettings* GetUserSettings() const;

	// 임시 변경을 엔진에 반영하고 저장한다 (검사를 통과한 뒤에만 부른다)
	void CommitChanges();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> RowBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UNU_CommonButtonWidget> ApplyButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UNU_CommonButtonWidget> DefaultsButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CloseButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GuideText;

	// 줄 WBP (UNUKeyBindingRowWidget 파생). WBP_KeyBindingsWindow 기본값에서 지정한다
	UPROPERTY(EditDefaultsOnly, Category = "NU|UI")
	TSubclassOf<UNUKeyBindingRowWidget> RowWidgetClass;

	// 묶음 제목 줄 WBP (UNUKeyBindingGroupWidget 파생). 미지정이면 제목 없이 줄만 이어 그린다
	UPROPERTY(EditDefaultsOnly, Category = "NU|UI")
	TSubclassOf<class UNUKeyBindingGroupWidget> GroupHeaderClass;

	// 아직 반영하지 않은 변경 (동작 이름 => 새 키). 빈 키는 "지정 안 됨"을 뜻한다. 키보드 칸 / 패드 칸
	TMap<FName, FKey> PendingKeys;
	TMap<FName, FKey> PendingPadKeys;

	// 키 입력을 기다리는 동작과 장치. 이름이 비어 있으면 대기 중이 아니다
	FName CapturingName;
	bool bCapturingGamepad = false;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UNUKeyBindingRowWidget>> RowByName;
};
