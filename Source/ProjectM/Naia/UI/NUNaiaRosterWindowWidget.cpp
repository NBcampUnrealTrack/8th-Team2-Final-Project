// Fill out your copyright notice in the Description page of Project Settings.

#include "Naia/UI/NUNaiaRosterWindowWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "CharacterState/NUPlayerState.h"
#include "CharacterState/NUSpiritState.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "InputCoreTypes.h"
#include "Inventory/UI/NUSheetEntryButton.h"
#include "Naia/Component/NUNaiaRosterComponent.h"
#include "Naia/Data/NUNaiaDataSubsystem.h"
#include "Naia/Data/NUNaiaDefinition.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"
#include "PaperFlipbook.h"
#include "PaperSprite.h"
#include "Styling/CoreStyle.h"
#include "Recovery/Net/NURecoveryNetSubsystem.h"

namespace
{
	// 파티 왼쪽, 보관함 오른쪽 구분용
	constexpr int32 RosterDisplayContainers[] =
	{
		NUNaiaContainer::Party,
		NUNaiaContainer::Storage
	};

	const FLinearColor EmptyCellColor(0.09f, 0.10f, 0.15f);
	const FLinearColor FilledCellColor(0.16f, 0.17f, 0.25f);
	const FLinearColor ActiveCellColor(0.20f, 0.26f, 0.20f);	// 파티 0번, 전투 참가
	const FLinearColor LockedCellColor(0.11f, 0.11f, 0.13f);	// 전투 중 잠김
	const FLinearColor SubTextColor(0.62f, 0.62f, 0.70f);
	const FLinearColor NameTextColor(0.92f, 0.92f, 0.95f);
	const FLinearColor LevelTextColor(0.85f, 0.80f, 0.55f);
	const FLinearColor ActiveTextColor(0.55f, 0.90f, 0.60f);
	const FLinearColor HealthBarColor(0.40f, 0.80f, 0.45f);
}

void UNUNaiaRosterWindowWidget::NativeConstruct()
{
	// 창을 다시 열면 초상화 재생을 처음부터 (베이스 NativeConstruct가 RefreshData까지 호출하므로 먼저)
	PortraitElapsed = 0.f;

	Super::NativeConstruct();
	
	if (UNUNaiaRosterComponent* RosterComponent = ResolveRoster())
	{
		RosterComponent->OnRosterChanged.RemoveDynamic(this, &UNUNaiaRosterWindowWidget::HandleRosterChanged);
		RosterComponent->OnRosterChanged.AddDynamic(this, &UNUNaiaRosterWindowWidget::HandleRosterChanged);
	}
}

void UNUNaiaRosterWindowWidget::NativeDestruct()
{
	if (Roster.IsValid())
	{
		Roster->OnRosterChanged.RemoveDynamic(this, &UNUNaiaRosterWindowWidget::HandleRosterChanged);
	}

	// 창과 별개로 뷰포트에 떠 있음
	// 창이 닫힐 때 같이 내림
	ClearDragVisual();

	Super::NativeDestruct();
}

void UNUNaiaRosterWindowWidget::HandleRosterChanged()
{
	RequestRefresh();
}

UNUNaiaRosterComponent* UNUNaiaRosterWindowWidget::ResolveRoster()
{
	if (Roster.IsValid())
	{
		return Roster.Get();
	}

	// 베이스의 ResolveComponents는 인벤·장비·ASC만 잡으므로 로스터는 직접 찾음
	const APlayerController* PC = GetOwningPlayer();
	ANUPlayerState* PS = PC ? Cast<ANUPlayerState>(PC->PlayerState) : nullptr;
	if (!IsValid(PS))
	{
		return nullptr;
	}

	Roster = PS->GetNaiaRosterComponent();
	return Roster.Get();
}

int32 UNUNaiaRosterWindowWidget::GetSlotCount(int32 Container) const
{
	if (!Roster.IsValid())
	{
		return 0;
	}

	return Container == NUNaiaContainer::Party
		? Roster->GetMaxPartySize()
		: Roster->GetMaxStorageSlots();
}

void UNUNaiaRosterWindowWidget::RefreshData()
{
	// ASC는 전투 중 판정에 필요함, 로스터는 별도로 잡음
	if (!ResolveComponents() || ResolveRoster() == nullptr)
	{
		return;
	}

	bInBattle = ASC->GetBattleEncounter() != nullptr;

	PartyEntries.Reset();
	StorageEntries.Reset();

	// 이름은 나이아 정의에서, 없으면 Id로 대신 표기
	const auto ResolveDisplayName = [this](int32 NaiaId) -> FText
	{
		const FNUNaiaDefinition* Definition = FindNaiaDefinition(NaiaId);
		if (Definition && !Definition->KoreanName.IsEmpty())
		{
			return Definition->KoreanName;
		}
		return FText::AsNumber(NaiaId);
	};

	// 파티는 액터에서 생성함, HP는 각 ASC가 복제하므로 클라도 읽을 수 있음
	const TArray<TObjectPtr<ANUSpiritState>>& Party = Roster->GetParty();
	for (int32 PartySlot = 0; PartySlot < Party.Num(); ++PartySlot)
	{
		const ANUSpiritState* Naia = Party[PartySlot].Get();
		if (!IsValid(Naia))
		{
			continue;
		}

		// 현재 파티슬롯만큼 파티창 정보 구조체를 생성함
		FNUNaiaSheetEntry& Entry = PartyEntries.AddDefaulted_GetRef();
		Entry.InstanceId = Naia->GetInstanceId();
		Entry.Container = NUNaiaContainer::Party;
		Entry.SlotNo = PartySlot;
		Entry.NaiaId = Naia->GetSpiritId();
		Entry.Level = Naia->GetCurrentLevel();
		Entry.DisplayName = ResolveDisplayName(Entry.NaiaId);
		Entry.bIsActive = PartySlot == 0;	// 파티슬롯 0번 활성화

		const UNUAbilitySystemComponent* NaiaASC = Naia->GetNUASC();
		if (const UNUVitalAttributeSet* Vital = IsValid(NaiaASC) ? NaiaASC->GetVitalAttributeSet() : nullptr)
		{
			Entry.CurrentHealth = Vital->GetHealth();
			Entry.MaxHealth = Vital->GetMaxHealth();
			Entry.HealthRatio = Entry.MaxHealth > KINDA_SMALL_NUMBER
				? FMath::Clamp(Entry.CurrentHealth / Entry.MaxHealth, 0.f, 1.f)
				: 0.f;
		}
	}

	// 보관함은 액터가 없어 수치를 구할 수 없으므로 체력 비율만 사용하고 숫자는 사용 X
	for (const FNUNaiaInstance& Instance : Roster->GetStorage())
	{
		FNUNaiaSheetEntry& Entry = StorageEntries.AddDefaulted_GetRef();
		Entry.InstanceId = Instance.InstanceId;
		Entry.Container = NUNaiaContainer::Storage;
		Entry.SlotNo = Instance.SlotNo;
		Entry.NaiaId = Instance.NaiaId;
		Entry.Level = Instance.Level;
		Entry.DisplayName = ResolveDisplayName(Entry.NaiaId);
		Entry.HealthRatio = FMath::Clamp(Instance.HealthRatio, 0.f, 1.f);
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		const UNURecoveryNetSubsystem* Net = GI->GetSubsystem<UNURecoveryNetSubsystem>();
		if (Net->IsResting())
		{
			for (FNUNaiaSheetEntry& Entry : PartyEntries) { Entry.HealthRatio = Net->GetDisplayedNaiaRatio(Entry.InstanceId, Entry.HealthRatio); Entry.CurrentHealth = Entry.MaxHealth * Entry.HealthRatio; }
			for (FNUNaiaSheetEntry& Entry : StorageEntries) { Entry.HealthRatio = Net->GetDisplayedNaiaRatio(Entry.InstanceId, Entry.HealthRatio); }
		}
	}
	PopulateRosterList();
	OnRosterListChanged();
}

const TArray<FNUNaiaSheetEntry>& UNUNaiaRosterWindowWidget::GetEntries(int32 Container) const
{
	return Container == NUNaiaContainer::Party ? PartyEntries : StorageEntries;
}

const FNUNaiaSheetEntry* UNUNaiaRosterWindowWidget::FindEntry(int32 Container, int32 SlotNo) const
{
	if (SlotNo == INDEX_NONE)
	{
		return nullptr;
	}

	return GetEntries(Container).FindByPredicate(
		[SlotNo](const FNUNaiaSheetEntry& Entry)
		{
			return Entry.SlotNo == SlotNo;
		});
}

int32 UNUNaiaRosterWindowWidget::FindFreeSlot(int32 Container) const
{
	const int32 SlotCount = GetSlotCount(Container);
	const TArray<FNUNaiaSheetEntry>& Entries = GetEntries(Container);

	for (int32 Candidate = 0; Candidate < SlotCount; ++Candidate)
	{
		const bool bUsed = Entries.ContainsByPredicate(
			[Candidate](const FNUNaiaSheetEntry& Entry)
			{
				return Entry.SlotNo == Candidate;
			}
			);

		if (!bUsed)
		{
			return Candidate;
		}
	}
	return INDEX_NONE;
}

bool UNUNaiaRosterWindowWidget::IsInteractionLocked() const
{
	return bInBattle;
}

void UNUNaiaRosterWindowWidget::PopulateRosterList()
{
	if (!IsValid(WidgetTree))
	{
		return;
	}

	// 셀과 애니메이션 등록은 매번 새로함, 아래에서 죽은 위젯을 가리키면 안 됨
	RecoveryHealthBars.Reset();
	RecoveryHealthTexts.Reset();
	CellByCoord.Reset();
	AnimatedPortraits.Reset();

	UPanelWidget* PartyPanel = FindPanelByName(TEXT("PartyListBox"));
	UPanelWidget* StoragePanel = FindPanelByName(TEXT("StorageListBox"));

	if (PartyPanel && StoragePanel)
	{
		PopulatePartyPanel(PartyPanel);
		PopulateStoragePanel(StoragePanel);
		return;
	}

	// 위에서 PartyPanel 이나 StorageListBox가 없을 경우 아래 처리, NaiaListBox 필요함
	// 파티도 보관함과 같은 그리드로 나란히 그림
	UPanelWidget* ListPanel = FindPanelByName(TEXT("NaiaListBox"));
	if (!ListPanel)
	{
		return;
	}

	UScrollBox* ScrollPanel = Cast<UScrollBox>(ListPanel);
	const float SavedScrollOffset = ScrollPanel ? ScrollPanel->GetScrollOffset() : 0.f;

	ListPanel->ClearChildren();

	UHorizontalBox* PanelRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(RosterDisplayContainers); ++Index)
	{
		UWidget* Panel = BuildContainerPanel(RosterDisplayContainers[Index]);
		if (UHorizontalBoxSlot* PanelSlot = Cast<UHorizontalBoxSlot>(PanelRow->AddChild(Panel)))
		{
			PanelSlot->SetPadding(FMargin(Index > 0 ? 16.f : 0.f, 0.f, 0.f, 0.f));
			PanelSlot->SetVerticalAlignment(VAlign_Top);
		}
	}
	ListPanel->AddChild(PanelRow);
	ListPanel->AddChild(BuildHintText());

	if (ScrollPanel)
	{
		ScrollPanel->SetScrollOffset(SavedScrollOffset);
	}
}

void UNUNaiaRosterWindowWidget::PopulatePartyPanel(UPanelWidget* Panel)
{
	Panel->ClearChildren();

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->AddChild(Column);

	Column->AddChild(BuildPanelHeader(NUNaiaContainer::Party));

	// 카드 4장 가로로, 빈 칸도 드롭 대상이라 정원만큼 전부 그림
	UHorizontalBox* CardRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	const int32 SlotCount = GetSlotCount(NUNaiaContainer::Party);
	for (int32 SlotNo = 0; SlotNo < SlotCount; ++SlotNo)
	{
		UWidget* Card = BuildPartyCard(SlotNo, FindEntry(NUNaiaContainer::Party, SlotNo));
		if (UHorizontalBoxSlot* CardSlot = Cast<UHorizontalBoxSlot>(CardRow->AddChild(Card)))
		{
			CardSlot->SetPadding(FMargin(SlotNo > 0 ? 4.f : 0.f, 0.f, 0.f, 0.f));
			CardSlot->SetVerticalAlignment(VAlign_Top);
		}
	}
	Column->AddChild(CardRow);

	// 안내는 파티 쪽에 배치
	if (UVerticalBoxSlot* HintSlot = Cast<UVerticalBoxSlot>(Column->AddChild(BuildHintText())))
	{
		HintSlot->SetPadding(FMargin(2.f, 12.f, 2.f, 0.f));
	}
}

void UNUNaiaRosterWindowWidget::PopulateStoragePanel(UPanelWidget* Panel)
{
	// 다시 생성하면 스크롤이 0으로 복귀하므로 위치를 기억했다 사용함
	UScrollBox* ScrollPanel = Cast<UScrollBox>(Panel);
	const float SavedScrollOffset = ScrollPanel ? ScrollPanel->GetScrollOffset() : 0.f;

	Panel->ClearChildren();

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->AddChild(Column);

	Column->AddChild(BuildPanelHeader(NUNaiaContainer::Storage));

	const int32 Columns = FMath::Max(1, GridColumns);
	const int32 SlotCount = GetSlotCount(NUNaiaContainer::Storage);

	UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>();
	Grid->SetSlotPadding(FMargin(2.f));
	for (int32 SlotNo = 0; SlotNo < SlotCount; ++SlotNo)
	{
		UWidget* Cell = BuildGridCell(NUNaiaContainer::Storage, SlotNo,
			FindEntry(NUNaiaContainer::Storage, SlotNo));
		if (UUniformGridSlot* GridSlot = Grid->AddChildToUniformGrid(Cell, SlotNo / Columns, SlotNo % Columns))
		{
			GridSlot->SetHorizontalAlignment(HAlign_Center);
			GridSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
	Column->AddChild(Grid);

	if (ScrollPanel)
	{
		ScrollPanel->SetScrollOffset(SavedScrollOffset);
	}
}

UTextBlock* UNUNaiaRosterWindowWidget::BuildPanelHeader(int32 Container) const
{
	const bool bIsParty = Container == NUNaiaContainer::Party;
	const int32 SlotCount = GetSlotCount(Container);
	const int32 Used = GetEntries(Container).Num();

	UTextBlock* Header = WidgetTree->ConstructWidget<UTextBlock>();
	Header->SetText(FText::Format(
		NSLOCTEXT("NUNaia", "RosterPanelHeader", "{0}  {1} / {2}"),
		bIsParty
			? NSLOCTEXT("NUNaia", "Party", "대기 목록")
			: NSLOCTEXT("NUNaia", "Storage", "보관함"),
		FText::AsNumber(Used),
		FText::AsNumber(SlotCount)));
	Header->SetColorAndOpacity(FSlateColor(Used >= SlotCount
		? FLinearColor(1.f, 0.45f, 0.4f)	// 가득 차면 붉게
		: SubTextColor)
		);
	Header->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 12));
	return Header;
}

UTextBlock* UNUNaiaRosterWindowWidget::BuildHintText() const
{
	// 전투 중에는 왜 안 되는지, 어디서 교체하는지 알려줌
	UTextBlock* HintText = WidgetTree->ConstructWidget<UTextBlock>();
	HintText->SetText(IsInteractionLocked()
		? NSLOCTEXT("NUNaia", "RosterHintBattle",
			"전투 중에는 편성을 바꿀 수 없습니다 · 전투 중 교체는 커맨드 메뉴의 [나이아 교체]")
		: NSLOCTEXT("NUNaia", "RosterHint",
			"드래그: 칸 이동/교환 · 우클릭: 반대쪽으로 보내기 · 맨 왼쪽이 전투에 참가합니다"));
	HintText->SetColorAndOpacity(FSlateColor(SubTextColor));
	HintText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 11));
	HintText->SetAutoWrapText(true);
	return HintText;
}

UWidget* UNUNaiaRosterWindowWidget::BuildContainerPanel(int32 Container)
{
	const bool bIsParty = Container == NUNaiaContainer::Party;

	UVerticalBox* Panel = WidgetTree->ConstructWidget<UVerticalBox>();

	// 패널 폭을 칸 수로 고정, 머리글 숫자 폭에 따라 창이 흔들리지 않게
	// 파티는 한 줄, 보관함만 여러 줄
	const int32 Columns = bIsParty ? 1 : FMath::Max(1, GridColumns);

	USizeBox* WidthLock = WidgetTree->ConstructWidget<USizeBox>();
	WidthLock->SetWidthOverride(Columns * (CellSize + 4.f) + 8.f);
	WidthLock->AddChild(Panel);

	const int32 SlotCount = GetSlotCount(Container);

	if (UVerticalBoxSlot* HeaderSlot = Cast<UVerticalBoxSlot>(Panel->AddChild(BuildPanelHeader(Container))))
	{
		HeaderSlot->SetPadding(FMargin(2.f, 0.f, 0.f, 2.f));
	}

	// 빈 칸까지 전부 그림, 빈칸도 드래그 대상이 되어야 함
	UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>();
	Grid->SetSlotPadding(FMargin(2.f));
	for (int32 SlotNo = 0; SlotNo < SlotCount; ++SlotNo)
	{
		UWidget* Cell = BuildGridCell(Container, SlotNo, FindEntry(Container, SlotNo));
		if (UUniformGridSlot* GridSlot = Grid->AddChildToUniformGrid(Cell, SlotNo / Columns, SlotNo % Columns))
		{
			GridSlot->SetHorizontalAlignment(HAlign_Center);
			GridSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
	Panel->AddChild(Grid);

	return WidthLock;
}

UWidget* UNUNaiaRosterWindowWidget::BuildGridCell(int32 Container, int32 SlotNo, const FNUNaiaSheetEntry* Entry)
{
	// 아이템 그리드의 셀을 그대로 재사용함, (컨테이너, 칸) 좌표만 들고 있는 범용 Border
	UNUInventoryCellBorder* Cell = WidgetTree->ConstructWidget<UNUInventoryCellBorder>();
	Cell->SetCellCoords(Container, SlotNo);
	Cell->OnCellMouseDown.BindUObject(this, &UNUNaiaRosterWindowWidget::HandleCellMouseDown);
	Cell->SetPadding(FMargin(0.f));
	Cell->SetClipping(EWidgetClipping::ClipToBounds);

	FLinearColor CellColor = EmptyCellColor;
	if (Entry)
	{
		CellColor = IsInteractionLocked()
			? LockedCellColor
			: Entry->bIsActive ? ActiveCellColor : FilledCellColor;
	}
	Cell->SetBrushColor(CellColor);
	CellByCoord.Add(FIntPoint(Container, SlotNo), Cell);

	if (Entry)
	{
		if (UWidget* Tooltip = BuildNaiaTooltip(*Entry))
		{
			Cell->SetToolTip(Tooltip);
		}
		Cell->AddChild(BuildCellContent(*Entry));
	}
	else
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(CellSize);
		Size->SetHeightOverride(CellSize);
		Cell->AddChild(Size);
	}

	return Cell;
}

USizeBox* UNUNaiaRosterWindowWidget::BuildCellContent(const FNUNaiaSheetEntry& Entry)
{
	// 그리드 셀과 드래그 비주얼이 같은 내용을 씀
	// 나이아 아이콘은 IconTexture가 아직 비어 있어 글자 표기로 감, 채워지면 여기만 바꾸면 됨
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetWidthOverride(CellSize);
	Size->SetHeightOverride(CellSize);

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	Size->AddChild(Box);

	// 파티 0번은 전투에 나가는 나이아라 표시를 줌
	if (Entry.bIsActive)
	{
		UTextBlock* ActiveText = WidgetTree->ConstructWidget<UTextBlock>();
		ActiveText->SetText(NSLOCTEXT("NUNaia", "ActiveMark", "전투"));
		ActiveText->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.9f, 0.6f)));
		ActiveText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 8));
		if (UVerticalBoxSlot* ActiveSlot = Cast<UVerticalBoxSlot>(Box->AddChild(ActiveText)))
		{
			ActiveSlot->SetPadding(FMargin(4.f, 2.f, 4.f, 0.f));
		}
	}

	UTextBlock* NameText = WidgetTree->ConstructWidget<UTextBlock>();
	NameText->SetText(Entry.DisplayName);
	NameText->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.92f, 0.95f)));
	NameText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 9));
	NameText->SetAutoWrapText(true);
	if (UVerticalBoxSlot* NameSlot = Cast<UVerticalBoxSlot>(Box->AddChild(NameText)))
	{
		NameSlot->SetPadding(FMargin(4.f, Entry.bIsActive ? 0.f : 4.f, 4.f, 0.f));
	}

	USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>();
	if (UVerticalBoxSlot* SpacerSlot = Cast<UVerticalBoxSlot>(Box->AddChild(Spacer)))
	{
		SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UTextBlock* LevelText = WidgetTree->ConstructWidget<UTextBlock>();
	LevelText->SetText(FText::Format(
		NSLOCTEXT("NUNaia", "CellLevel", "Lv.{0}"), FText::AsNumber(Entry.Level)));
	LevelText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.8f, 0.55f)));
	LevelText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 9));
	if (UVerticalBoxSlot* LevelSlot = Cast<UVerticalBoxSlot>(Box->AddChild(LevelText)))
	{
		LevelSlot->SetPadding(FMargin(4.f, 0.f, 4.f, 1.f));
	}

	// 체력 비율만 얇은 바로, 숫자는 파티 카드에만 있음
	{
		UProgressBar* HealthBar = WidgetTree->ConstructWidget<UProgressBar>();
		HealthBar->SetPercent(Entry.HealthRatio);
		RecoveryHealthBars.Add(Entry.InstanceId, HealthBar);
		HealthBar->SetFillColorAndOpacity(FLinearColor(0.4f, 0.8f, 0.45f));

		USizeBox* BarSize = WidgetTree->ConstructWidget<USizeBox>();
		BarSize->SetHeightOverride(4.f);
		BarSize->AddChild(HealthBar);

		if (UVerticalBoxSlot* BarSlot = Cast<UVerticalBoxSlot>(Box->AddChild(BarSize)))
		{
			BarSlot->SetPadding(FMargin(4.f, 0.f, 4.f, 4.f));
		}
	}

	return Size;
}

UWidget* UNUNaiaRosterWindowWidget::BuildPartyCard(int32 SlotNo, const FNUNaiaSheetEntry* Entry)
{
	// 카드도 그리드 셀과 같은 Border 사용 - 크기만 다르고 드래그·우클릭 동일하게 작동
	UNUInventoryCellBorder* Card = WidgetTree->ConstructWidget<UNUInventoryCellBorder>();
	Card->SetCellCoords(NUNaiaContainer::Party, SlotNo);
	Card->OnCellMouseDown.BindUObject(this, &UNUNaiaRosterWindowWidget::HandleCellMouseDown);
	Card->SetPadding(FMargin(0.f));
	Card->SetClipping(EWidgetClipping::ClipToBounds);
	CellByCoord.Add(FIntPoint(NUNaiaContainer::Party, SlotNo), Card);

	FLinearColor CardColor = EmptyCellColor;
	if (Entry)
	{
		CardColor = IsInteractionLocked()
			? LockedCellColor
			: Entry->bIsActive ? ActiveCellColor : FilledCellColor;
	}
	Card->SetBrushColor(CardColor);

	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetWidthOverride(PartyCardWidth);
	Size->SetHeightOverride(PartyCardHeight);
	Card->AddChild(Size);

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	Size->AddChild(Box);

	// 정보가 없다면 빈칸
	if (!Entry)
	{
		UTextBlock* EmptyText = WidgetTree->ConstructWidget<UTextBlock>();
		EmptyText->SetText(NSLOCTEXT("NUNaia", "EmptyPartySlot", "비어 있음"));
		EmptyText->SetColorAndOpacity(FSlateColor(SubTextColor));
		EmptyText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 10));
		EmptyText->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* EmptySlot = Cast<UVerticalBoxSlot>(Box->AddChild(EmptyText)))
		{
			EmptySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			EmptySlot->SetHorizontalAlignment(HAlign_Center);
			EmptySlot->SetVerticalAlignment(VAlign_Center);
		}
		return Card;
	}

	Card->SetToolTip(BuildNaiaTooltip(*Entry));

	// 파티 0번 전투 참가 배지
	// 빈 글자는 높이가 0이라 배지 없는 카드의 초상화가 위로 밀림, 높이를 고정해 줄을 맞춤
	UTextBlock* ActiveText = WidgetTree->ConstructWidget<UTextBlock>();
	ActiveText->SetText(Entry->bIsActive
		? NSLOCTEXT("NUNaia", "ActiveMark", "전투 참가")
		: FText::GetEmpty());
	ActiveText->SetColorAndOpacity(FSlateColor(ActiveTextColor));
	ActiveText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 9));
	ActiveText->SetJustification(ETextJustify::Center);

	USizeBox* BadgeSize = WidgetTree->ConstructWidget<USizeBox>();
	BadgeSize->SetHeightOverride(14.f);
	BadgeSize->AddChild(ActiveText);
	if (UVerticalBoxSlot* ActiveSlot = Cast<UVerticalBoxSlot>(Box->AddChild(BadgeSize)))
	{
		ActiveSlot->SetPadding(FMargin(4.f, 4.f, 4.f, 2.f));
	}

	// 초상화
	UPaperFlipbook* Flipbook = nullptr;
	if (UObject* Portrait = ResolvePortraitResource(Entry->NaiaId, Flipbook))
	{
		UImage* PortraitImage = BuildPortraitImage(Portrait);
		if (UVerticalBoxSlot* PortraitSlot = Cast<UVerticalBoxSlot>(Box->AddChild(PortraitImage)))
		{
			PortraitSlot->SetHorizontalAlignment(HAlign_Center);
			PortraitSlot->SetVerticalAlignment(VAlign_Center);
			PortraitSlot->SetPadding(FMargin(2.f, 0.f, 2.f, 4.f));
		}

		if (Flipbook)
		{
			FNUNaiaPortraitAnim& Anim = AnimatedPortraits.AddDefaulted_GetRef();
			Anim.Image = PortraitImage;
			Anim.Flipbook = Flipbook;
			Anim.Duration = Flipbook->GetTotalDuration();
		}
	}

	UTextBlock* NameText = WidgetTree->ConstructWidget<UTextBlock>();
	NameText->SetText(Entry->DisplayName);
	NameText->SetColorAndOpacity(FSlateColor(NameTextColor));
	NameText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 12));
	NameText->SetJustification(ETextJustify::Center);
	NameText->SetAutoWrapText(true);
	if (UVerticalBoxSlot* NameSlot = Cast<UVerticalBoxSlot>(Box->AddChild(NameText)))
	{
		NameSlot->SetPadding(FMargin(4.f, 0.f, 4.f, 0.f));
	}

	UTextBlock* LevelText = WidgetTree->ConstructWidget<UTextBlock>();
	LevelText->SetText(FText::Format(
		NSLOCTEXT("NUNaia", "CellLevel", "Lv.{0}"), FText::AsNumber(Entry->Level)));
	LevelText->SetColorAndOpacity(FSlateColor(LevelTextColor));
	LevelText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 11));
	LevelText->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* LevelSlot = Cast<UVerticalBoxSlot>(Box->AddChild(LevelText)))
	{
		LevelSlot->SetPadding(FMargin(4.f, 2.f, 4.f, 0.f));
	}

	// HP, 속성이 카드 아래에 붙게 남는 세로 공간 조절
	USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>();
	if (UVerticalBoxSlot* SpacerSlot = Cast<UVerticalBoxSlot>(Box->AddChild(Spacer)))
	{
		SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	if (Entry->MaxHealth > KINDA_SMALL_NUMBER)
	{
		UTextBlock* HealthText = WidgetTree->ConstructWidget<UTextBlock>();
		RecoveryHealthTexts.Add(Entry->InstanceId, HealthText);
		HealthText->SetText(FText::Format(
			NSLOCTEXT("NUNaia", "CellHealth", "{0} / {1}"),
			FText::AsNumber(FMath::CeilToInt(Entry->CurrentHealth)),
			FText::AsNumber(FMath::CeilToInt(Entry->MaxHealth))));
		HealthText->SetColorAndOpacity(FSlateColor(SubTextColor));
		HealthText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 9));
		HealthText->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* HealthSlot = Cast<UVerticalBoxSlot>(Box->AddChild(HealthText)))
		{
			HealthSlot->SetPadding(FMargin(4.f, 0.f, 4.f, 1.f));
		}

		UProgressBar* HealthBar = WidgetTree->ConstructWidget<UProgressBar>();
		HealthBar->SetPercent(Entry->HealthRatio);
		RecoveryHealthBars.Add(Entry->InstanceId, HealthBar);
		HealthBar->SetFillColorAndOpacity(HealthBarColor);

		USizeBox* BarSize = WidgetTree->ConstructWidget<USizeBox>();
		BarSize->SetHeightOverride(5.f);
		BarSize->AddChild(HealthBar);

		if (UVerticalBoxSlot* BarSlot = Cast<UVerticalBoxSlot>(Box->AddChild(BarSize)))
		{
			BarSlot->SetPadding(FMargin(4.f, 0.f, 4.f, 4.f));
		}
	}

	// 속성 태그가 없으면 줄을 만들지 않음
	if (const FNUNaiaDefinition* Definition = FindNaiaDefinition(Entry->NaiaId))
	{
		if (Definition->ElementTag.IsValid())
		{
			UTextBlock* ElementText = WidgetTree->ConstructWidget<UTextBlock>();
			ElementText->SetText(FText::FromName(Definition->ElementTag.GetTagLeafName()));
			ElementText->SetColorAndOpacity(FSlateColor(SubTextColor));
			ElementText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 9));
			ElementText->SetJustification(ETextJustify::Center);
			if (UVerticalBoxSlot* ElementSlot = Cast<UVerticalBoxSlot>(Box->AddChild(ElementText)))
			{
				ElementSlot->SetPadding(FMargin(4.f, 0.f, 4.f, 4.f));
			}
		}
	}

	return Card;
}

UObject* UNUNaiaRosterWindowWidget::ResolvePortraitResource(int32 NaiaId, UPaperFlipbook*& OutFlipbook) const
{
	OutFlipbook = nullptr;

	// AnimationClass 는 PaperZD 라 별도 Flipbook 컬럼 필요함
	// 플립북이 있으면 첫 프레임을 돌려주고, 재생은 호출부가 OutFlipbook 으로 등록
	if (const FNUNaiaDefinition* Definition = FindNaiaDefinition(NaiaId))
	{
		if (UPaperFlipbook* Flipbook = Definition->PreviewFlipbook.LoadSynchronous())
		{
			OutFlipbook = Flipbook;
			return Flipbook->GetSpriteAtFrame(0);
		}

		// 움직일 필요가 없는 나이아는 텍스처만 지정해도 됨
		if (UTexture2D* Icon = Definition->IconTexture.LoadSynchronous())
		{
			return Icon;
		}
	}

	// 에셋 지정안하면 기본 초상화 사용
	UObject* Dummy = DefaultPortrait.LoadSynchronous();
	if (UPaperFlipbook* Flipbook = Cast<UPaperFlipbook>(Dummy))
	{
		OutFlipbook = Flipbook;
		return Flipbook->GetSpriteAtFrame(0);
	}
	return Dummy;	// 스프라이트 또는 텍스처, 정지 이미지
}

UImage* UNUNaiaRosterWindowWidget::BuildPortraitImage(UObject* Resource) const
{
	// Slate 브러시는 플립북을 직접 그리지 못함
	// 매 프레임마다 갈아끼우는 방식으로 그려야 함 
	// 스프라이트는 그대로 리소스로 들어감
	UImage* Portrait = WidgetTree->ConstructWidget<UImage>();

	FSlateBrush Brush;
	Brush.SetResourceObject(Resource);

	FVector2D SourceSize(64.f, 64.f);
	if (const UPaperSprite* Sprite = Cast<UPaperSprite>(Resource))
	{
		SourceSize = Sprite->GetSlateAtlasData().GetSourceDimensions();
	}
	else if (const UTexture2D* Texture = Cast<UTexture2D>(Resource))
	{
		SourceSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
	}

	// 원본 비율을 유지한 채 높이만 맞춤
	// 높이 조절은 처음 한번만 처리해야 함
	const float Scale = SourceSize.Y > 0.f ? PortraitHeight / SourceSize.Y : 1.f;
	Brush.ImageSize = SourceSize * Scale;

	Portrait->SetBrush(Brush);
	return Portrait;
}

const FNUNaiaDefinition* UNUNaiaRosterWindowWidget::FindNaiaDefinition(int32 NaiaId) const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UNUNaiaDataSubsystem* Subsystem =
		GameInstance ? GameInstance->GetSubsystem<UNUNaiaDataSubsystem>() : nullptr;

	return IsValid(Subsystem) ? Subsystem->FindDefinitionById(NaiaId) : nullptr;
}

UWidget* UNUNaiaRosterWindowWidget::BuildNaiaTooltip(const FNUNaiaSheetEntry& Entry) const
{
	if (!IsValid(WidgetTree))
	{
		return nullptr;
	}

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();

	UTextBlock* NameText = WidgetTree->ConstructWidget<UTextBlock>();
	NameText->SetText(FText::Format(
		NSLOCTEXT("NUNaia", "TipName", "{0}  Lv.{1}"),
		Entry.DisplayName, FText::AsNumber(Entry.Level)));
	NameText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 12));
	Box->AddChild(NameText);

	// 속성은 정의에서 읽음, 태그가 비어 있으면 줄을 만들지 않음
	if (const FNUNaiaDefinition* Definition = FindNaiaDefinition(Entry.NaiaId))
	{
		if (Definition->ElementTag.IsValid())
		{
			UTextBlock* ElementText = WidgetTree->ConstructWidget<UTextBlock>();
			ElementText->SetText(FText::Format(
				NSLOCTEXT("NUNaia", "TipElement", "속성 : {0}"),
				FText::FromName(Definition->ElementTag.GetTagLeafName())));
			ElementText->SetColorAndOpacity(FSlateColor(SubTextColor));
			ElementText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 11));
			Box->AddChild(ElementText);
		}
	}

	return Box;
}

void UNUNaiaRosterWindowWidget::QuickMove(int32 FromContainer, int32 FromSlotNo)
{
	if (!Roster.IsValid())
	{
		return;
	}

	// 반대 컨테이너로 보냄, 파티 -> 보관함 / 보관함 -> 파티
	const int32 ToContainer = FromContainer == NUNaiaContainer::Party
		? NUNaiaContainer::Storage
		: NUNaiaContainer::Party;

	// 반대쪽에 빈공간이 없다면 종료
	const int32 FreeSlot = FindFreeSlot(ToContainer);
	if (FreeSlot == INDEX_NONE)
	{
		return;
	}

	Roster->ServerMoveNaia(FromContainer, FromSlotNo, ToContainer, FreeSlot);
}

FEventReply UNUNaiaRosterWindowWidget::HandleCellMouseDown(int32 Container, int32 SlotNo,
	const FPointerEvent& MouseEvent)
{
	// 전투 중에는 서버가 거절함 
	// 이때 창이 조작을 받으면 아무 일도 안 일어나는 것처럼 보임
	if (IsInteractionLocked())
	{
		return FEventReply(true);
	}

	const FNUNaiaSheetEntry* Entry = FindEntry(Container, SlotNo);

	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && Entry)
	{
		QuickMove(Container, SlotNo);
		return FEventReply(true);
	}

	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && Entry)
	{
		// 드래그 후보 등록, 실제 시작은 커서가 움직여야 옴
		PendingDragContainer = Container;
		PendingDragSlotNo = SlotNo;
		return UWidgetBlueprintLibrary::DetectDragIfPressed(MouseEvent, this, EKeys::LeftMouseButton);
	}

	// 빈 칸이나 기타 버튼, 창 드래그로 번지지 않게
	return FEventReply(true);
}

void UNUNaiaRosterWindowWidget::NativeOnDragDetected(const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

	const FNUNaiaSheetEntry* Entry = FindEntry(PendingDragContainer, PendingDragSlotNo);
	if (!Entry)
	{
		return;
	}

	UNUNaiaDragOperation* Operation = NewObject<UNUNaiaDragOperation>(this);
	Operation->SourceContainer = PendingDragContainer;
	Operation->SourceSlotNo = PendingDragSlotNo;
	Operation->OnDrop.AddDynamic(this, &UNUNaiaRosterWindowWidget::HandleDragEnded);
	Operation->OnDragCancelled.AddDynamic(this, &UNUNaiaRosterWindowWidget::HandleDragEnded);
	OutOperation = Operation;

	// 생성 / 커서 추적 / 정리는 베이스가 담당함, 내용만 만들어 넘김
	// 카드는 커서에 붙기엔 커서 그리드 셀 크기로 만듦
	ShowDragVisual(BuildCellContent(*Entry), CellSize);
}

bool UNUNaiaRosterWindowWidget::NativeOnDrop(const FGeometry& InGeometry,
	const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	UNUNaiaDragOperation* Operation = Cast<UNUNaiaDragOperation>(InOperation);
	if (!Operation)
	{
		// 아이템 드래그가 이 창에 떨어진 경우, 받지 않고 넘김
		return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	}

	PendingDragSlotNo = INDEX_NONE;
	ClearDragVisual();

	const FVector2D ScreenPos = InDragDropEvent.GetScreenSpacePosition();

	// 셀 위에 놓으면 이동/교환, 빈칸이면 이동 찬칸이면 교환은 서버의 MoveNaia가 판단함
	for (const TPair<FIntPoint, TWeakObjectPtr<UNUInventoryCellBorder>>& Pair : CellByCoord)
	{
		const UNUInventoryCellBorder* Cell = Pair.Value.Get();
		if (!Cell || !Cell->GetCachedGeometry().IsUnderLocation(ScreenPos))
		{
			continue;
		}

		const int32 TargetContainer = Pair.Key.X;
		const int32 TargetSlotNo = Pair.Key.Y;
		const bool bSameSlot = Operation->SourceContainer == TargetContainer
			&& Operation->SourceSlotNo == TargetSlotNo;

		if (!bSameSlot && Roster.IsValid())
		{
			Roster->ServerMoveNaia(
				Operation->SourceContainer, Operation->SourceSlotNo, TargetContainer, TargetSlotNo);
		}
		return true;
	}

	// 창 안 기타 영역, 아무 일도 없음
	return true;
}

void UNUNaiaRosterWindowWidget::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	Super::NativeOnDragCancelled(InDragDropEvent, InOperation);

	PendingDragSlotNo = INDEX_NONE;
	ClearDragVisual();

	// 아이템 창과 달리 창 밖에 놓으면 취소, 버리기 기능 X
}

void UNUNaiaRosterWindowWidget::HandleDragEnded(UDragDropOperation* InOperation)
{
	PendingDragSlotNo = INDEX_NONE;
	ClearDragVisual();
}

void UNUNaiaRosterWindowWidget::RefreshRecoveryDisplay()
{
	const UNURecoveryNetSubsystem* Net = GetGameInstance()->GetSubsystem<UNURecoveryNetSubsystem>();
	for (TArray<FNUNaiaSheetEntry>* Entries : { &PartyEntries, &StorageEntries })
	{
		for (FNUNaiaSheetEntry& Entry : *Entries)
		{
			Entry.HealthRatio = Net->GetDisplayedNaiaRatio(Entry.InstanceId, Entry.HealthRatio);
			Entry.CurrentHealth = Entry.MaxHealth * Entry.HealthRatio;
			if (const TWeakObjectPtr<UProgressBar>* Bar = RecoveryHealthBars.Find(Entry.InstanceId); Bar && Bar->IsValid()) { Bar->Get()->SetPercent(Entry.HealthRatio); }
			if (const TWeakObjectPtr<UTextBlock>* Text = RecoveryHealthTexts.Find(Entry.InstanceId); Text && Text->IsValid())
			{
				Text->Get()->SetText(FText::Format(NSLOCTEXT("NUNaia", "CellHealth", "{0} / {1}"), FText::AsNumber(FMath::CeilToInt(Entry.CurrentHealth)), FText::AsNumber(FMath::CeilToInt(Entry.MaxHealth))));
			}
		}
	}
}

void UNUNaiaRosterWindowWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	// 드래그 비주얼의 커서 추적은 베이스가 함, 여기서는 초상화 재생만 처리
	Super::NativeTick(MyGeometry, InDeltaTime);
	RecoveryDisplayElapsed += InDeltaTime;
	if (RecoveryDisplayElapsed >= 1.0f)
	{
		RecoveryDisplayElapsed = 0.0f;
		if (UGameInstance* GI = GetGameInstance(); GI && GI->GetSubsystem<UNURecoveryNetSubsystem>()->IsResting()) { RefreshRecoveryDisplay(); }
	}

	if (AnimatedPortraits.IsEmpty())
	{
		return;
	}

	PortraitElapsed += InDeltaTime;
	for (const FNUNaiaPortraitAnim& Anim : AnimatedPortraits)
	{
		UImage* Image = Anim.Image.Get();
		const UPaperFlipbook* Flipbook = Anim.Flipbook.Get();
		if (!Image || !Flipbook || Anim.Duration <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		// GetSpriteAtTime 은 총 길이를 넘으면 마지막 프레임에 멈춤 
		// mod 연산으로 직접 재생해야 함
		if (UPaperSprite* Frame = Flipbook->GetSpriteAtTime(FMath::Fmod(PortraitElapsed, Anim.Duration)))
		{
			// 브러시 크기는 그대로 두고 플립북에서 따온 이미지로 교체
			Image->SetBrushResourceObject(Frame);
		}
	}
}


void UNUNaiaRosterWindowWidget::CloseSelf()
{
	if (UNUNaiaRosterComponent* RosterComponent = ResolveRoster())
	{
		RosterComponent->ToggleRosterUI();	// 열림 상태 토글 = 닫기
	}
}
