// 인벤토리 디버그 콘솔 명령 (nu.Inv.*) — 개발 전용, Shipping 제외.
// 등록 방식은 self-registering static (FAutoConsoleCommandWithWorldAndArgs).

#if !UE_BUILD_SHIPPING

#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "CharacterState/NUPlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/NUEquipmentComponent.h"
#include "Inventory/NUInventoryComponent.h"
#include "Inventory/NUInventoryDebugOverlay.h"
#include "Inventory/NUInventoryTypes.h"

namespace NUInventoryDebug
{

using NUInventoryDebugUI::FindLocalInventory;

static FAutoConsoleCommandWithWorldAndArgs GAddCmd(
	TEXT("nu.Inv.Add"),
	TEXT("아이템 추가: nu.Inv.Add <RowName> [Count]  (예: nu.Inv.Add Potion_SmallHeal 3)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogNUInventory, Warning, TEXT("[nu.Inv.Add] 사용법: nu.Inv.Add <RowName> [Count]"));
			return;
		}
		UNUInventoryComponent* Inventory = FindLocalInventory(World);
		if (Inventory == nullptr)
		{
			return;
		}
		const FName RowName(*Args[0]);
		const int32 Count = Args.Num() >= 2 ? FCString::Atoi(*Args[1]) : 1;
		// 서버 RPC — 리슨 호스트에서는 즉시 로컬 실행, 클라에서는 서버로 라우팅
		Inventory->ServerDebugAddItem(RowName, Count);
	}));

static FAutoConsoleCommandWithWorldAndArgs GUseCmd(
	TEXT("nu.Inv.Use"),
	TEXT("아이템 사용: nu.Inv.Use <RowName|Index>  (예: nu.Inv.Use Potion_SmallHeal / nu.Inv.Use 0)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogNUInventory, Warning, TEXT("[nu.Inv.Use] 사용법: nu.Inv.Use <RowName|Index>"));
			return;
		}
		UNUInventoryComponent* Inventory = FindLocalInventory(World);
		if (Inventory == nullptr)
		{
			return;
		}

		const FNUItemInstance* Instance = nullptr;
		if (Args[0].IsNumeric())
		{
			const int32 Index = FCString::Atoi(*Args[0]);
			if (Inventory->GetItems().IsValidIndex(Index))
			{
				Instance = &Inventory->GetItems()[Index];
			}
		}
		else
		{
			Instance = Inventory->FindFirstByRowName(FName(*Args[0]));
		}

		if (Instance == nullptr)
		{
			UE_LOG(LogNUInventory, Warning, TEXT("[nu.Inv.Use] 아이템 못 찾음: %s"), *Args[0]);
			return;
		}
		Inventory->ServerUseItem(Instance->InstanceId);
	}));

static FAutoConsoleCommandWithWorldAndArgs GDropCmd(
	TEXT("nu.Inv.Drop"),
	TEXT("아이템 버리기(스택 전체): nu.Inv.Drop <RowName|Index>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogNUInventory, Warning, TEXT("[nu.Inv.Drop] 사용법: nu.Inv.Drop <RowName|Index>"));
			return;
		}
		UNUInventoryComponent* Inventory = FindLocalInventory(World);
		if (Inventory == nullptr)
		{
			return;
		}

		const FNUItemInstance* Instance = nullptr;
		if (Args[0].IsNumeric())
		{
			const int32 Index = FCString::Atoi(*Args[0]);
			if (Inventory->GetItems().IsValidIndex(Index))
			{
				Instance = &Inventory->GetItems()[Index];
			}
		}
		else
		{
			Instance = Inventory->FindFirstByRowName(FName(*Args[0]));
		}

		if (Instance == nullptr)
		{
			UE_LOG(LogNUInventory, Warning, TEXT("[nu.Inv.Drop] 아이템 못 찾음: %s"), *Args[0]);
			return;
		}
		Inventory->ServerDropItem(Instance->InstanceId, Instance->Count);
	}));

static FAutoConsoleCommandWithWorldAndArgs GListCmd(
	TEXT("nu.Inv.List"),
	TEXT("로컬 인벤토리 복제본 출력 (소유자 전용 복제 확인용)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		UNUInventoryComponent* Inventory = FindLocalInventory(World);
		if (Inventory == nullptr)
		{
			return;
		}
		const TArray<FNUItemInstance>& Items = Inventory->GetItems();
		UE_LOG(LogNUInventory, Log, TEXT("[nu.Inv.List] %d개 항목 (%d/%d칸)"),
			Items.Num(), Inventory->GetUsedSlots(), Inventory->GetMaxSlots());
		for (int32 i = 0; i < Items.Num(); ++i)
		{
			const FNUItemInstance& Item = Items[i];
			const FNUItemDefRow* Def = Inventory->FindItemDef(Item.ItemRowName);
			UE_LOG(LogNUInventory, Log, TEXT("  [%d] 칸 %d  %s (%s) x%d  id=%s"),
				i,
				Item.SlotNo,
				*Item.ItemRowName.ToString(),
				Def ? *Def->DisplayName.ToString() : TEXT("정의 없음"),
				Item.Count,
				*Item.InstanceId.ToString(EGuidFormats::DigitsWithHyphens).Left(8));
		}
	}));

// 창고 창만 바로 연다 (패드 조작 확인용 --- 창고 상자 상호작용은 서버 검증을 거친다). 사용: nu.Storage.Open
static FAutoConsoleCommandWithWorldAndArgs GStorageOpenCmd(
	TEXT("nu.Storage.Open"),
	TEXT("창고 창 토글 (개발용 --- 서버 상호작용 없이 UI만)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		if (UNUInventoryComponent* Inventory = FindLocalInventory(World))
		{
			Inventory->ToggleStorageUI();
		}
	}));

// 서버가 아직 구간 표를 안 주므로 서버 없는 PIE에서 탭별 칸을 시험한다 (서버에 요청한 시작 번호 0 / 100 / 200 / 300).
// 사용: nu.Inv.Sections [구간 칸 수, 기본 30] --- 가득 찬 탭을 보려면 칸 수를 작게. 되돌리려면 PIE를 다시 시작
static FAutoConsoleCommandWithWorldAndArgs GSectionsCmd(
	TEXT("nu.Inv.Sections"),
	TEXT("분류 구간 시험 (서버 없는 PIE 전용): nu.Inv.Sections [칸 수=30] --- 장비 0 / 소모품 100 / 재료 200 / 기타 300부터 나누고 가방 아이템을 제 구간으로 옮긴다"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		UNUInventoryComponent* Inventory = FindLocalInventory(World);
		if (Inventory == nullptr)
		{
			return;
		}
		if (Inventory->IsServerInventoryActive())
		{
			UE_LOG(LogNUInventory, Warning, TEXT("[nu.Inv.Sections] 서버 인벤은 서버가 칸을 정한다 --- 서버 없는 PIE에서만 쓴다"));
			return;
		}
		const int32 Count = Args.Num() >= 1 ? FMath::Clamp(FCString::Atoi(*Args[0]), 1, 100) : 30;
		TArray<FNUBagSection> Sections;
		for (int32 Category = 0; Category < NUBagCategory::Num; ++Category)
		{
			Sections.Add({ Category * 100, Count });
		}
		Inventory->SetBagSections(Sections);

		// 배포 때 서버가 한 번 옮기는 것을 흉내 낸다 --- 제 구간 밖의 아이템을 원래 칸 순서대로 그 구간의 앞 빈 칸으로
		TArray<FNUItemInstance> Items = Inventory->GetItems(); // 옮기는 동안 원본 배열이 바뀐다
		Items.Sort([](const FNUItemInstance& A, const FNUItemInstance& B) { return A.SlotNo < B.SlotNo; });
		for (const FNUItemInstance& Item : Items)
		{
			const FNUBagSection& Section = Sections[NUItem::GetBagCategory(Inventory->FindItemDef(Item.ItemRowName))];
			if (Section.Contains(Item.SlotNo))
			{
				continue;
			}
			int32 FreeSlotNo = Section.Start;
			while (FreeSlotNo < Section.Start + Section.Count && Inventory->FindBySlotNo(FreeSlotNo) != nullptr)
			{
				++FreeSlotNo;
			}
			if (FreeSlotNo >= Section.Start + Section.Count)
			{
				UE_LOG(LogNUInventory, Warning, TEXT("[nu.Inv.Sections] %s(칸 %d) --- 구간이 가득 차 못 옮김"), *Item.ItemRowName.ToString(), Item.SlotNo);
				continue;
			}
			Inventory->ServerMoveItemBetween(NUContainer::Inventory, Item.SlotNo, NUContainer::Inventory, FreeSlotNo);
		}
		UE_LOG(LogNUInventory, Log, TEXT("[nu.Inv.Sections] 구간마다 %d칸"), Count);
	}));

// 보상을 못 받는 일은 서버 가방 구간이 가득 차야 생긴다 --- 안내(ShowRewardMissingNotice)만 따로 시험한다.
// 사용: nu.Inv.RewardMissing 6615x1 6001x3 --- 항목은 퀘스트 완료 응답의 Reward.MissingItems와 같은 꼴
static FAutoConsoleCommandWithWorldAndArgs GRewardMissingCmd(
	TEXT("nu.Inv.RewardMissing"),
	TEXT("못 받은 보상 안내 시험: nu.Inv.RewardMissing [6615x1 ...] --- 퀘스트 완료 응답의 Reward.MissingItems를 흉내 낸다"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		if (UNUInventoryComponent* Inventory = FindLocalInventory(World))
		{
			Inventory->ShowRewardMissingNotice(FString::Printf(TEXT("{\"Reward\":{\"MissingItems\":[%s]}}"),
				*FString::JoinBy(Args, TEXT(","), [](const FString& Arg) { return FString::Printf(TEXT("\"%s\""), *Arg); })));
		}
	}));

// 서버 전투는 서버가 있어야 시작된다 --- 가방·장비·통합 메뉴의 전투 반응(열린 창 닫기, 버튼 끄기, 거절 문구)만 따로 시험한다.
// 사용: nu.Inv.FakeNetBattle 1 (시작) / nu.Inv.FakeNetBattle 0 (끝)
static FAutoConsoleCommandWithWorldAndArgs GFakeNetBattleCmd(
	TEXT("nu.Inv.FakeNetBattle"),
	TEXT("서버 전투 중인 것처럼 굴리기: nu.Inv.FakeNetBattle [1|0] --- 가방·장비·통합 메뉴의 전투 반응만 시험한다"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		if (UNUInventoryComponent* Inventory = FindLocalInventory(World))
		{
			Inventory->DebugSetFakeNetBattle(!Args.IsValidIndex(0) || Args[0].ToBool());
		}
	}));

static UNUEquipmentComponent* FindLocalEquipment(UWorld* World)
{
	UNUInventoryComponent* Inventory = NUInventoryDebugUI::FindLocalInventory(World);
	const ANUPlayerState* PS = Inventory ? Cast<ANUPlayerState>(Inventory->GetOwner()) : nullptr;
	return PS ? PS->GetEquipmentComponent() : nullptr;
}

static FAutoConsoleCommandWithWorldAndArgs GEquipCmd(
	TEXT("nu.Inv.Equip"),
	TEXT("장비 장착: nu.Inv.Equip <RowName>  (예: nu.Inv.Equip E_DEV_01)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogNUInventory, Warning, TEXT("[nu.Inv.Equip] 사용법: nu.Inv.Equip <RowName>"));
			return;
		}
		UNUInventoryComponent* Inventory = NUInventoryDebugUI::FindLocalInventory(World);
		UNUEquipmentComponent* Equipment = FindLocalEquipment(World);
		if (Inventory == nullptr || Equipment == nullptr)
		{
			return;
		}
		const FNUItemInstance* Instance = Inventory->FindFirstByRowName(FName(*Args[0]));
		if (Instance == nullptr)
		{
			UE_LOG(LogNUInventory, Warning, TEXT("[nu.Inv.Equip] 인벤토리에 없음: %s"), *Args[0]);
			return;
		}
		Equipment->ServerEquipItem(Instance->InstanceId);
	}));

static FAutoConsoleCommandWithWorldAndArgs GUnequipCmd(
	TEXT("nu.Inv.Unequip"),
	TEXT("장비 해제: nu.Inv.Unequip <슬롯인덱스 0~7> (투구0 갑옷1 장갑2 신발3 무기4 반지1=5 반지2=6 귀걸이7)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1 || !Args[0].IsNumeric())
		{
			UE_LOG(LogNUInventory, Warning, TEXT("[nu.Inv.Unequip] 사용법: nu.Inv.Unequip <슬롯인덱스 0~10>"));
			return;
		}
		UNUEquipmentComponent* Equipment = FindLocalEquipment(World);
		const int32 SlotIndex = FCString::Atoi(*Args[0]);
		if (Equipment == nullptr || SlotIndex < 0 || SlotIndex >= static_cast<int32>(ENUEquipSlot::MAX))
		{
			return;
		}
		Equipment->ServerUnequipSlot(static_cast<ENUEquipSlot>(SlotIndex));
	}));

static FAutoConsoleCommandWithWorldAndArgs GUICmd(
	TEXT("nu.Inv.UI"),
	TEXT("임시 인벤토리 오버레이 토글 (Slate — 정식 UMG UI 전까지)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		NUInventoryDebugUI::Toggle(World);
	}));

} // namespace NUInventoryDebug

#endif // !UE_BUILD_SHIPPING
