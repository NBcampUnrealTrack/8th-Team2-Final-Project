// 아이템을 그리는 UI들이 같이 쓰는 함수 --- 옛 시트 창(소지품·장비·상점·거래·공방)과 통합 캐릭터 메뉴의 아이템 페이지(인벤토리·장비).
// 두 쪽의 부모 클래스가 달라(UNUSheetWidgetBase / 메뉴 페이지) 상속으로는 나눠 쓸 수 없어 함수로 뺐다.
// 글자 규칙(분류·필요 레벨·효과 수치)도 여기 한 곳에 둔다 --- 툴팁과 상세 카드가 같은 말을 해야 한다
#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Templates/SubclassOf.h"

class APlayerController;
class UNUConfirmDialog;
class UTexture2D;
struct FNUEquipStats;
struct FNUItemDefRow;

// 장비 스탯 한 줄
struct FNUEquipStatEntry
{
	FText Label;
	float Value = 0.f;
	bool bPercent = false; // 0~1 비율로 저장된 값 --- 표기할 때 퍼센트로 환산한다
};

namespace NUItemUI
{
	// 이름 규칙({ItemIconFolder}/T_Item_{RowName})으로 아이콘 텍스처를 찾는다.
	// 없으면 nullptr --- 호출부가 글자 표기로 폴백한다 (아이콘 미제작 아이템 대비).
	// bFamilyFallback: 장비(EQ_<계열>_<등급>)에 제 그림이 없으면 같은 계열의 다른 등급 그림을 쓴다 --- 등급을 따로 보여 주는
	// 화면(통합 메뉴의 등급 선·상세 카드)에서만 켠다. 이름 글자가 등급을 가르는 유일한 표시인 칸(옛 창·거래)은 끈 채로 둔다
	PROJECTM_API UTexture2D* LoadItemIcon(FName ItemRowName, bool bFamilyFallback = false);

	// 장비 스탯 전부를 표시 순서대로 (0인 것도 포함 --- 비교 표시는 한쪽만 0이어도 줄을 남긴다).
	// 어느 컬럼이 비율인지는 기획 시트가 정한다 --- ATK_Bonus·MaxHP_Bonus가 비율, DEF_Bonus는 정수
	PROJECTM_API TArray<FNUEquipStatEntry> GetEquipStatEntries(const FNUEquipStats& Stats);

	// "+11.2%" / "+12" --- 비율은 소수 한 자리, 정수 스탯은 그대로
	PROJECTM_API FString FormatEquipStatValue(float Value, bool bPercent);

	// "장비 · 무기" / "소모품" ... (장비면 부위까지)
	PROJECTM_API FText GetCategoryText(const FNUItemDefRow& Def);

	// "필요 레벨 N" --- 장비이고 요구 레벨이 있을 때만, 아니면 빈 글자. 1레벨도 표시한다(8/24 결정), 0은 개발 더미라 숨긴다
	PROJECTM_API FText GetRequiredLevelText(const FNUItemDefRow& Def);

	// "{직업} 전용" --- 직업 전용 장비만, 아니면 빈 글자
	PROJECTM_API FText GetRequiredJobText(const FNUItemDefRow& Def);

	// 소모품 효과 수치 "50" / "30%" / "50 · 30%" --- 표의 Effect1/2. 상태 해제는 값이 상태이상 ID라 수치로 보이지 않는다
	PROJECTM_API FString GetEffectValuesText(const FNUItemDefRow& Def);

	// 소모품을 어디서 쓸 수 있는지 ("필드에서만 사용 가능" ...)
	PROJECTM_API FText GetUsageText(const FNUItemDefRow& Def);

	// 장착 요청 --- 다른 직업 전용 장비는 보내지 않고 알린다 (서버는 직업 제한을 모른다)
	PROJECTM_API void EquipChecked(const APlayerController* Player, const FGuid& InstanceId);

	// 되돌릴 수 없는 동작 앞에 확인 창을 세운다. DialogClass가 비면 프로젝트 세팅(NU UI)의 공용 확인 창,
	// 그것도 없으면 경고만 남기고 바로 실행한다 (개발 중 동작이 죽지 않게)
	PROJECTM_API void RequestConfirm(APlayerController* Player, TSubclassOf<UNUConfirmDialog> DialogClass,
		const FText& Message, const FText& ConfirmLabel, FSimpleDelegate OnConfirmed);

	// 스택 전체 버리기 --- 확인 창을 거친다
	PROJECTM_API void RequestDrop(APlayerController* Player, const FGuid& InstanceId, TSubclassOf<UNUConfirmDialog> DialogClass = nullptr);
}
