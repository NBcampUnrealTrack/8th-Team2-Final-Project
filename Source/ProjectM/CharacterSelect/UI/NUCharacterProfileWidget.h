// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CharacterSelect/NUCharacterTypes.h"
#include "Inventory/UI/NUSheetWidgetBase.h"
#include "NUCharacterProfileWidget.generated.h"

// 남의 캐릭터 정보창 --- 우클릭 [캐릭터 정보]로 연다 (UNUCharacterProfileUIController 소유).
// 서버 CharacterProfile(142) 스냅샷 1회분을 그린다: 이름·레벨·직업 + [장비 확인] 슬롯 그리드.
// 내 정보창(NUCharacterInfoWidget)과 달리 스탯·미리보기가 없다 --- 서버가 의도적으로 안 준다.
//
// 명명 계약: NameText / LevelText / JobText (내 정보창과 동일 --- 이름으로 찾는다).
// 위젯 계약(BindWidget): EquipToggleButton([장비 확인]), EquipListBox(슬롯 그리드가 붙는 패널, 기본 Collapsed)
UCLASS(Abstract)
class PROJECTM_API UNUCharacterProfileWidget : public UNUSheetWidgetBase
{
	GENERATED_BODY()

public:
	// 컨트롤러가 응답을 받아 넘긴다. 열려 있는 창에 다시 부르면 내용만 갈린다
	void SetProfile(const FNUCharacterProfile& InProfile);

protected:
	virtual void NativeConstruct() override; // EquipToggleButton 바인딩
	virtual void RefreshData() override;
	virtual void CloseSelf() override;

private:
	// 이름·레벨·직업을 명명 계약 텍스트에 넣는다. 직업은 성장 데이터 표에서 표기를 찾는다
	void PopulateProfile();

	// "EquipListBox"에 부위 9칸 그리드를 만들고 칸은 WBP_ItemCell로 채운다. 아이콘·툴팁은 아이템 표에서 해석한다
	void PopulateEquipGrid();
	class UWidget* BuildEquipCell(ENUEquipSlot InSlot);

	// [장비 확인] --- 그리드 패널을 접었다 편다
	UFUNCTION()
	void HandleEquipToggleClicked();

	FNUCharacterProfile Profile;

	// 장비 칸 픽셀 크기 --- "작게 슬롯칸" 기획이라 장비창(72)보다 작다. WBP 클래스 기본값에서 조정
	UPROPERTY(EditAnywhere, Category = "NU|Sheet", meta = (ClampMin = 32.f))
	float SlotCellSize = 48.f;

	// 순정 Button(레거시) / UNU_CommonButtonWidget(WBP_CommonButton) 둘 다 지원한다 --- UWidget으로
	// 받아 캐스팅한다. 클릭 바인딩은 NativeConstruct가 실제 타입을 캐스팅해 처리한다
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UWidget> EquipToggleButton;

	// 그리드가 붙는 패널. 타입을 패널로 고정하지 않는다 --- WBP가 세로/격자 중 무엇을 쓰든 접고 펴기만 한다
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UWidget> EquipListBox;
};
