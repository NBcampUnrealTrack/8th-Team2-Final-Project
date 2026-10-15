// Fill out your copyright notice in the Description page of Project Settings.

#include "CharacterSelect/UI/NUCharacterProfileWidget.h"

#include "Blueprint/WidgetTree.h"
#include "CharacterProgression/Data/NUProgressionDataSubsystem.h"
#include "CharacterSelect/UI/NUCharacterProfileUIController.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Engine/GameInstance.h"
#include "Inventory/NUInventoryComponent.h"
#include "Inventory/NUInventoryTypes.h"
#include "Inventory/UI/NUItemCellWidget.h"
#include "Styling/CoreStyle.h"
#include "UI/Common/NU_CommonButtonWidget.h"
#include "UI/NUUIManager.h"
#include "UI/NUUISettings.h"

void UNUCharacterProfileWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// EquipToggleButton은 순정 Button(레거시) / UNU_CommonButtonWidget(WBP_CommonButton) 둘 다 지원한다.
	// 재오픈 중복 구독 방지 --- 해제 선행 후 구독
	if (UButton* ToggleButtonPure = Cast<UButton>(EquipToggleButton))
	{
		ToggleButtonPure->OnClicked.RemoveDynamic(this, &UNUCharacterProfileWidget::HandleEquipToggleClicked);
		ToggleButtonPure->OnClicked.AddDynamic(this, &UNUCharacterProfileWidget::HandleEquipToggleClicked);
	}
	else if (UNU_CommonButtonWidget* ToggleCommonButton = Cast<UNU_CommonButtonWidget>(EquipToggleButton))
	{
		ToggleCommonButton->OnCommonClicked.RemoveDynamic(this, &UNUCharacterProfileWidget::HandleEquipToggleClicked);
		ToggleCommonButton->OnCommonClicked.AddDynamic(this, &UNUCharacterProfileWidget::HandleEquipToggleClicked);
	}
}

void UNUCharacterProfileWidget::SetProfile(const FNUCharacterProfile& InProfile)
{
	Profile = InProfile;

	// 아이템 표 해석(아이콘·이름·툴팁)에 내 인벤 컴포넌트를 쓴다 --- 표는 모두에게 같다
	ResolveComponents();
	PopulateProfile();
	PopulateEquipGrid();
}

void UNUCharacterProfileWidget::RefreshData()
{
	// 서버 스냅샷 1회분이라 다시 받을 것이 없다 --- 들고 있는 값으로 다시 그린다 (베이스 갱신 경로용)
	PopulateProfile();
	PopulateEquipGrid();
}

void UNUCharacterProfileWidget::PopulateProfile()
{
	if (UTextBlock* NameText = FindTextByName(TEXT("NameText")))
	{
		NameText->SetText(FText::FromString(Profile.Name));
	}
	if (UTextBlock* LevelText = FindTextByName(TEXT("LevelText")))
	{
		LevelText->SetText(FText::Format(
			NSLOCTEXT("NUCharacterProfile", "LevelFormat", "Lv. {0}"), FText::AsNumber(Profile.Level)));
	}

	// 직업은 번호만 와서 표기를 성장 데이터 표에서 찾는다 (내 정보창과 같은 방식).
	// 전직 정보는 응답에 없어 기본 직업명까지만 적는다
	FText JobName = NSLOCTEXT("NUCharacterProfile", "Adventurer", "모험가");
	const UGameInstance* GameInstance = GetGameInstance();
	const UNUProgressionDataSubsystem* Data = GameInstance != nullptr
		? GameInstance->GetSubsystem<UNUProgressionDataSubsystem>()
		: nullptr;
	if (Data != nullptr && Profile.JobId > 0)
	{
		if (const FNUJobDefinitionRow* Job = Data->FindJob(Profile.JobId))
		{
			JobName = Job->DisplayName;
		}
	}
	if (UTextBlock* JobText = FindTextByName(TEXT("JobText")))
	{
		JobText->SetText(JobName);
	}
}

void UNUCharacterProfileWidget::PopulateEquipGrid()
{
	UPanelWidget* Panel = FindPanelByName(TEXT("EquipListBox"));
	if (Panel == nullptr || WidgetTree == nullptr)
	{
		return;
	}
	Panel->ClearChildren();

	// 3열 --- 몸에 걸치는 것 / 손발에 드는 것 / 장신구를 한 줄씩
	static const TArray<ENUEquipSlot> Order = {
		ENUEquipSlot::Head,    ENUEquipSlot::Chest, ENUEquipSlot::Legs,
		ENUEquipSlot::Weapon,  ENUEquipSlot::Gloves, ENUEquipSlot::Boots,
		ENUEquipSlot::Earring, ENUEquipSlot::Ring1, ENUEquipSlot::Ring2 };
	constexpr int32 ColumnCount = 3;

	UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>();
	Grid->SetSlotPadding(FMargin(2.f));
	for (int32 Index = 0; Index < Order.Num(); ++Index)
	{
		UWidget* Cell = BuildEquipCell(Order[Index]);
		if (UUniformGridSlot* GridSlot = Grid->AddChildToUniformGrid(Cell, Index / ColumnCount, Index % ColumnCount))
		{
			GridSlot->SetHorizontalAlignment(HAlign_Center);
			GridSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
	Panel->AddChild(Grid);
}

UWidget* UNUCharacterProfileWidget::BuildEquipCell(ENUEquipSlot InSlot)
{
	// 응답의 부위 키는 서버 번호다 (클라 enum 값과 순서가 다르다)
	const int32* FoundItemId = Profile.Equipment.Find(NUEquip::ToServerEquipSlot(InSlot));
	const FName RowName = (FoundItemId != nullptr && Inventory.IsValid())
		? Inventory->FindRowNameByNumericId(*FoundItemId)
		: NAME_None;
	const FNUItemDefRow* Def = RowName != NAME_None ? Inventory->FindItemDef(RowName) : nullptr;

	// 장비창 칸과 같은 WBP_ItemCell --- 클릭 동작은 없다 (남의 장비)
	UNUItemCellWidget* Cell = MakeItemCell(SlotCellSize);
	if (Cell == nullptr)
	{
		return nullptr;
	}

	if (FoundItemId == nullptr)
	{
		Cell->SetEmpty(NUEquip::GetSlotDisplayName(InSlot)); // 빈 칸은 어느 부위인지 보여 준다
		return Cell;
	}

	// 아이콘 없는 장비는 이름, 표에 없는 장비는 번호를 글자로 보여 준다
	Cell->SetItem(RowName != NAME_None ? LoadItemIconTexture(RowName) : nullptr, 0,
		Def != nullptr ? Def->DisplayName : FText::AsNumber(*FoundItemId, &FNumberFormattingOptions::DefaultNoGrouping()), false);
	if (RowName != NAME_None)
	{
		if (UWidget* Tooltip = BuildItemTooltip(RowName))
		{
			Cell->SetToolTip(Tooltip);
		}
	}
	return Cell;
}

void UNUCharacterProfileWidget::HandleEquipToggleClicked()
{
	EquipListBox->SetVisibility(EquipListBox->IsVisible() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
}

void UNUCharacterProfileWidget::CloseSelf()
{
	UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer());
	UNUCharacterProfileUIController* Controller = UI != nullptr
		? UI->GetController<UNUCharacterProfileUIController>()
		: nullptr;
	if (Controller != nullptr)
	{
		Controller->CloseProfileWindow();
	}
}
