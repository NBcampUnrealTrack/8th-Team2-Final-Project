// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory/NUInventoryTypes.h"
#include "Net/EmberServerManager.h" // EEmberConnState (UFUNCTION 시그니처)
#include "NUInventoryComponent.generated.h"

class APlayerController;
class FJsonObject;
class UDataTable;
class UNUCharacterInfoWidget;
class UNUEquipmentWidget;
class UNUInventoryWindowWidget;
class UNUSheetWidgetBase;
enum class ENUCharacterMenuSection : uint8;
struct FGameplayTag;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnNUItemAcquired, FName, int32);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnNUPurchaseSucceeded, int32);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnNUStorageAccessed, int32);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNUInventoryChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNUStorageChanged, int32, Container);

// 서버 저장소 번호 (gRPC_help — InventoryList/ItemMove의 container 값과 1:1)
namespace NUContainer
{
	constexpr int32 Inventory = 1;
	constexpr int32 CharStorage = 2;    // 캐릭터 창고
	constexpr int32 AccountStorage = 3; // 계정 창고

	// 장착 중인 장비. 칸 번호가 곧 부위(ENUEquipSlot 0~7)라 별도 복원 패킷 없이
	// InventoryList(4) 한 번으로 장착 상태를 그대로 받는다
	constexpr int32 Equipment = 4;

	// 클라 전용 --- 거래 창 내 제안 칸의 좌표·패드로 집은 거래 칸. 서버 저장소 번호가 아니다 (칸 번호 = 거래 칸 번호)
	constexpr int32 TradeOffer = -1;
}

// 패드로 집은 칸 (SlotNo가 INDEX_NONE이면 든 것 없음)
struct FNUPadHeldItem
{
	int32 Container = 0;
	int32 SlotNo = INDEX_NONE;
	FGuid InstanceId;

	bool IsSet() const { return SlotNo != INDEX_NONE; }
};

// 플레이어 인벤토리 (NUPlayerState 부착).
// 서버 권위: 상태 변경(Add/Remove/Use)은 전부 서버에서만
// 복제: FastArray 델타 + COND_OwnerOnly (본인 클라에만 보임 - 거래/파티 노출은 추후 별도 요약 복제)
// 정의 데이터: ItemDefTable(DataTable) 미지정 시 런타임 더미 폴백 
// 영속성 없음(PlayerState 수명) - 추후 gRPC 게임서버 저장 연동 지점
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTM_API UNUInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNUInventoryComponent();

	// 목록 갱신과 구분되는 실제 획득·구매·창고 접근 결과. NPC 인자는 NpcId다.
	FOnNUItemAcquired OnItemAcquired;
	FOnNUPurchaseSucceeded OnPurchaseSucceeded;
	FOnNUStorageAccessed OnStorageAccessed;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// -- 서버 권위 API (HasAuthority 가드) --
	// 기존 스택을 채우고 넘치면 새 인스턴스(NewGuid) 생성
	bool AddItem(FName ItemRowName, int32 Count = 1);

	// 보상 전체가 들어갈 때만 지급한다. 가방 부족으로 일부 보상이 사라지는 것을 막는다.
	bool CanReceiveItems(const TMap<FName, int32>& Items) const;
	bool AddItemsAtomic(const TMap<FName, int32>& Items, TFunction<bool()> BeforeNotify = {});

	// Count 차감, 0 도달 시 항목 제거
	bool RemoveItem(const FGuid& InstanceId, int32 Count = 1);

	// 장착 이동용(서버 전용): 인스턴스 1개를 목록에서 꺼내 스냅샷 반환 — InstanceId 보존이 목적.
	// AddItem(FName)과 달리 새 GUID를 만들지 않는다 (장착<=>해제 왕복에 필수)
	bool RemoveInstanceForEquip(const FGuid& InstanceId, FNUItemInstance& OutItem);
	bool AddItemInstance(const FNUItemInstance& Item);

	// -- 용량 (스택 하나 = 한 칸) --
	UFUNCTION(BlueprintPure, Category = "NU|Inventory")
	int32 GetUsedSlots() const { return InventoryList.Items.Num(); }

	UFUNCTION(BlueprintPure, Category = "NU|Inventory")
	int32 GetMaxSlots() const { return MaxSlots; }

	// 그 아이템이 들어갈 빈 칸이 있는지 (분류 구간이 있으면 그 아이템의 구간에서)
	UFUNCTION(BlueprintPure, Category = "NU|Inventory")
	bool HasFreeSlot(FName ItemRowName) const;

	// -- 분류 구간 (9/30 회의 "탭별 상한", 서버에 요청 중) --- 분류마다 가방 칸 번호를 따로 쓴다.
	//    구간이 없으면 가방 전체가 한 통이다 (지금 서버) --

	// 그 분류(NUBagCategory)의 구간. 구간이 없는 가방이면 nullptr
	const FNUBagSection* GetBagSection(int32 Category) const;

	// 그 아이템이 드는 분류 (NUBagCategory). 서버가 구간과 함께 준 분류 표(Types)가 있으면 그 표를, 없으면 클라 표(NUItem::GetBagCategory)를 쓴다
	int32 GetBagCategory(const FNUItemDefRow* Def) const;

	// 분류 구간을 바꾼다 (번호 = NUBagCategory, 비우면 한 통). 구간이 있으면 가방 칸 수는 구간 칸 수의 합이다
	void SetBagSections(const TArray<FNUBagSection>& Sections);

	// -- 조회 (서버/소유 클라) --
	const FNUItemInstance* FindInstance(const FGuid& InstanceId) const;
	const FNUItemInstance* FindFirstByRowName(FName ItemRowName) const;
	const FNUItemInstance* FindBySlotNo(int32 SlotNo) const;
	int32 GetCountByItemId(FName ItemRowName) const;
	const TArray<FNUItemInstance>& GetItems() const { return InventoryList.Items; }
	const FNUItemDefRow* FindItemDef(FName ItemRowName) const;

	// 서버가 보내는 숫자 id를 테이블 행 이름으로 (거래 칸처럼 내 인벤 밖 아이템을 그릴 때 쓴다)
	FName FindRowNameByNumericId(int32 NumericId) const;

	// 숫자 id로 정의를 바로 찾는다 — 이름만 필요한 곳(넷 전투 행동 배너)이 쓴다.
	// 정의는 DataTable 행이라 그 아이템을 안 들고 있어도, 다 쓰고 없어진 뒤에도 유효하다
	const FNUItemDefRow* FindItemDefByNumericId(int32 NumericId) const;

	// 전투 연동 계약: NUBattleEncounter가 Item 커맨드(FNUBattleCommandRequest.InstanceId) 검증·실행 시 사용
	bool CanUseInBattle(const FGuid& InstanceId) const;

	// -- 사용 (소유 클라 => 서버) --
	UFUNCTION(Server, Reliable)
	void ServerUseItem(FGuid InstanceId);

	// -- 버리기 (소유 클라 => 서버). 퀘스트 아이템은 거절 --
	// 게임서버 연동 시 이 RPC 몸통이 gRPC 요청으로 대체된다 (호출부·UI는 그대로)
	UFUNCTION(Server, Reliable)
	void ServerDropItem(FGuid InstanceId, int32 Count);

	// -- 칸 이동 (소유 클라 => 서버). ItemMove(61)와 같은 판정: 빈칸=이동, 같은 아이템(스택 가능)=합치기(상한까지 채우고 나머지는 출발 칸에), 그 밖의 찬칸=교환 --
	// 컨테이너가 달라도 된다 (인벤 <=> 창고). 창고가 끼면 서버 전용 — 미로그인 시 무동작
	UFUNCTION(Server, Reliable)
	void ServerMoveItemBetween(int32 FromContainer, int32 FromSlotNo, int32 ToContainer, int32 ToSlotNo);

	// 디버그 전용 (nu.Inv.Add 콘솔 명령 경유, Shipping에서는 무시)
	UFUNCTION(Server, Reliable)
	void ServerDebugAddItem(FName ItemRowName, int32 Count);

	// FNUInventoryList::PostReplicatedReceive => 클라 측 변경 알림
	void HandleInventoryReplicated();

	// --- 게임서버(gRPC) 연동 — InventoryList(60)/ItemMove(61)/SVR_Item_Slot(70) ---
	// 서버에 인벤(container=1) 목록 요청. 미로그인 시 무동작
	void RequestServerInventory();

	// 창고 포함 임의 컨테이너 목록 요청 (2=캐릭터 창고, 3=계정 창고 — 서버 전용, 로컬 폴백 없음)
	void RequestContainerList(int32 Container);

	// UEmberServerManager가 호출 (응답/푸시 수신)
	void HandleServerListResponse(int32 ResultCode, const FString& ResponseData);
	void HandleServerMoveResponse(int32 ResultCode);
	void HandleServerSortResponse(int32 ResultCode);
	void HandleServerSplitResponse(int32 ResultCode);
	void HandleServerDiscardResponse(int32 ResultCode);

	// 상점 구매/판매 응답 (180/181). 200이면 {"Gold": 잔액}, 바뀐 칸은 SVR_Item_Slot으로 따로 온다.
	// 2208(금액 불일치)은 result_message에 서버 금액이 실려 온다 --- 클라 표가 낡았다는 신호
	void HandleServerShopResponse(bool bBuy, int32 ResultCode, const FString& ResultMessage, const FString& ResponseData);
	void HandleServerSlotPush(int32 Container, int32 SlotNo, int32 ItemId, int32 ItemCount);

	// --- 창고 조회 (UI) — container 1이면 인벤을 그대로 반환 ---
	const TArray<FNUItemInstance>* GetContainerItems(int32 Container) const;
	int32 GetContainerSlotCount(int32 Container) const;
	bool IsContainerLoaded(int32 Container) const; // 창고: 목록 응답을 받았는지 (인벤은 항상 true)

	// 대상 저장소에서 그 아이템이 들어갈 빈 칸 중 가장 앞 (없거나 미로드면 INDEX_NONE) — 우클릭 빠른 이동용.
	// 가방은 아이템의 분류 구간에서 찾는다
	int32 FindFreeSlotInContainer(int32 Container, FName ItemRowName) const;

	// 창고 창(좌 계정 | 우 캐릭터 두 패널) 열기/닫기 — 필드 창고 오브젝트 상호작용이 호출
	UFUNCTION(BlueprintCallable, Category = "NU|Inventory")
	void ToggleStorageUI();

	UFUNCTION(BlueprintPure, Category = "NU|Inventory")
	bool IsStorageOpen() const;

	// 소지품 열기/닫기 (I키 — NUPlayerController가 호출). 혼자 열 때는 통합 캐릭터 메뉴의 인벤토리 페이지,
	// 다른 창 옆에서 주고받을 때(창고·상점·공방이 연 것, 거래 중)는 옛 소지품 창이다 --- 통합 메뉴는 화면을 다 덮어 옆에 둘 수 없다.
	// 옛 창 WBP 미지정 시 디버그 오버레이 폴백
	UFUNCTION(BlueprintCallable, Category = "NU|Inventory")
	void ToggleInventoryUI();

	// 통합 캐릭터 메뉴 여닫기 (패드 Y --- NUPlayerController가 호출). 닫혀 있으면 캐릭터 정보 페이지로 열고, 떠 있으면 어느 페이지든 닫는다.
	// 패드는 이 메뉴의 키가 하나뿐이라 키보드 I처럼 "다른 페이지면 인벤토리로 넘기기"를 하면 닫으려던 키에 페이지가 바뀐다.
	// 옛 소지품 창을 쓰는 때(창고·상점·거래 옆)는 ToggleInventoryUI와 같다
	UFUNCTION(BlueprintCallable, Category = "NU|Inventory")
	void ToggleCharacterMenuUI();

	// Hub equipment entry uses the unified menu; companion equipment windows keep their existing behavior.
	void ToggleEquipmentMenuUI();

	// 떠 있는 통합 캐릭터 메뉴를 닫는다 (전투·거래가 시작될 때처럼 메뉴를 둘 수 없는 때)
	void CloseCharacterMenu();

	// 이 가방의 주인이 전투 중인가 --- 로컬 전투(복제되는 Encounter) 또는 이 클라이언트의 서버 전투(전투 무대가 내려가기 시작할 때까지).
	// 서버 전투는 이 클라이언트만 아는 상태라 내 플레이어에게만 성립한다 (리슨 서버가 다른 플레이어의 요청을 처리할 때는 Encounter만 본다).
	// 가방·장비·통합 메뉴가 전투 여부를 물을 때는 Encounter를 직접 보지 말고 이것을 쓴다 --- 서버 전투에서는 Encounter가 비어 있다
	bool IsOwnerInBattle() const;

	// 위 값이 바뀌었을 때 (내 플레이어에서만 울린다). 전투가 시작된 쪽이면 열린 창을 닫은 뒤에 알린다
	FSimpleMulticastDelegate OnOwnerBattleChanged;

#if !UE_BUILD_SHIPPING
	// 시험용 (콘솔 nu.Inv.FakeNetBattle) --- 서버 없이 서버 전투 중인 것처럼 굴린다
	void DebugSetFakeNetBattle(bool bFake);
#endif

	// 인벤 정렬 --- 기준(ENUItemSortMode, 기본은 종류 => 아이템 번호 => 개수 많은 순)대로 앞칸부터 채운다.
	// 최종 배치를 ItemSort 한 발로 보낸다. 정렬 기준은 클라 DataTable에만 있어 서버가 정렬할 수 없고,
	// 서버는 아이템별 총량과 칸별 스택 상한만 검증한다.
	// 같은 아이템이라도 스택을 합치지 않는다 — 나누기 기능과 상충하므로 나란히 놓기만 한다.
	// 거래 중에는 거부 --- 재배치로 칸 번호가 미끄러지면 올려둔 것과 다른 아이템이 나간다.
	// 돌려주는 값: 요청이 받아들여졌는지 (정렬할 것이 없을 때도 참). 거절 사유는 배너로 알리고, 앞 정렬의 응답을 기다리는 중이면 조용히 거짓.
	// 서버가 나중에 거절하는 것(총량 불일치 등)은 여기서 알 수 없다 --- 그때는 중단 배너가 뜬다
	UFUNCTION(BlueprintCallable, Category = "NU|Inventory")
	bool SortInventory(ENUItemSortMode Mode = ENUItemSortMode::Kind);

	// 창고(2/3)도 같은 규칙으로 정렬한다. 창고는 복제가 없어 서버 없이는 재배치할 수 없다
	UFUNCTION(BlueprintCallable, Category = "NU|Inventory")
	bool SortContainer(int32 Container, ENUItemSortMode Mode = ENUItemSortMode::Kind);

	// 장착/해제 거절을 배너로 알린다 (gRPC 가이드 코드표 기준) --- EmberServerManager 응답 핸들러가 부른다.
	// 로그만 남기면 화면에서는 소리 없는 실패로 보인다
	void ShowEquipRejectNotice(int32 ResultCode, bool bUnequip);

	// 아이템 요청 거절을 배너로 알린다 (이동·나누기·목록 등 사유가 코드로만 오는 경로).
	// 장착/해제는 실패 문구가 따로 있어 ShowEquipRejectNotice를 쓴다
	void ShowItemRejectNotice(int32 ResultCode);

	// 보상 응답(퀘스트 완료 103의 response_data)의 Reward.MissingItems를 읽어, 가방 구간이 가득 차 못 받은 보상이 있으면 배너로 알린다.
	// 서버에 우편함이 없어 못 받은 보상은 사라진다 (가이드 13.6) --- 응답을 받는 쪽이 부른다
	void ShowRewardMissingNotice(const FString& ResponseData);

	// 저장소 이름 (안내 문구용)
	static FText GetContainerDisplayName(int32 Container);

	// 서버가 인벤의 원천인지 (한 번이라도 서버 목록에 아이템이 왔는지)
	bool IsServerInventoryActive() const { return bServerInventoryActive; }

	UFUNCTION(BlueprintPure, Category = "NU|Inventory")
	bool IsSorting() const { return bSorting; }

	// 스택 나누기 — 한 칸에서 Count개를 떼어 빈 칸으로 옮긴다. 목적지는 서버가 고른다.
	// 전부 떼는 건 나누기가 아니라 이동이라 거부한다 (그건 드래그로 한다).
	// 스택이 안 되는 아이템(장비 등)도 거부
	UFUNCTION(BlueprintCallable, Category = "NU|Inventory")
	void SplitStack(int32 Container, int32 SlotNo, int32 Count);

	// 캐릭터 정보 열기/닫기 (P키 — NUPlayerController가 호출). 통합 캐릭터 메뉴의 캐릭터 정보 페이지 (셸 미지정이면 옛 정보창)
	UFUNCTION(BlueprintCallable, Category = "NU|Inventory")
	void ToggleCharacterInfoUI();

	// 장비창 토글. 인벤 창을 여닫으면 ToggleInventoryUI가 이 창도 따라 여닫는다 (8/25 회의 결정)
	UFUNCTION(BlueprintCallable, Category = "NU|Inventory")
	void ToggleEquipmentUI();

	// --- 상점 (기획 1차: 일반 상점 구매·판매) ---
	// 서버에 화폐·상점 패킷이 아직 없어 로컬 더미로 동작한다 (요청 중 --- Docs/상점요청_2026-09-01.md).
	// 서버 인벤 모드에서는 총량이 어긋나므로 차단된다 (버리기 차단과 같은 이유)

	// 소지금. 서버 인벤을 받을 때 목록 응답(CharacterList)의 잔액으로 맞춰진다 --- 그 전엔 더미 초기값
	UFUNCTION(BlueprintPure, Category = "NU|Shop")
	int64 GetGold() const { return Gold; }

	// 골드 가감. 수입은 사망 부채부터 상환하고 일반 소비는 부채를 늘리지 않는다
	UFUNCTION(BlueprintCallable, Category = "NU|Shop")
	void AddGold(int64 Delta);
	void ApplyServerGold(int64 ServerGold);
	bool ApplyDeathGoldLoss(int64 Loss);

	// 상점 창 열기/닫기 --- NPC 상호작용(또는 임시 콘솔 nu.Shop.Open)이 부른다.
	// 키는 기획 표 Shop_ID (NPC 표의 Shop_ID 컬럼과 같은 번호).
	// 창고처럼 인벤 창을 같이 띄우고, 닫을 때 같이 닫는다
	UFUNCTION(BlueprintCallable, Category = "NU|Shop")
	void OpenShopUI(int32 ShopId);

	UFUNCTION(BlueprintCallable, Category = "NU|Shop")
	void CloseShopUI();

	// 지금 열려 있는 상점의 Shop_ID (닫혀 있으면 0) --- 인벤 우클릭 판매가 매각가 기준으로 쓴다
	UFUNCTION(BlueprintPure, Category = "NU|Shop")
	int32 GetOpenShopId() const;

	// 이 아이템을 지금 가방이 받을 수 있는 최대 수량 (기존 스택 여유 + 빈 칸)
	int32 GetReceivableCount(FName ItemRowName) const;

	// 구매/판매. 서버 인벤이 켜져 있으면 180/181로 보내고 결과는 HandleServerShopResponse가 받는다. 서버 없이 PIE면 로컬 더미.
	// 실패 사유는 알림 배너로 알린다
	bool ShopBuyItem(const struct FNUShopItemRow& Item, int32 Count, const struct FNUShopRow& Shop);
	bool ShopSellItem(const FGuid& InstanceId, int32 Count, const struct FNUShopRow& Shop);

	// 공방 제작 --- 재료와 골드를 확인하고 결과물을 Times번 몫만큼 지급한다.
	// 서버 제작 패킷이 아직 없어 항상 로컬에서 돈다 (상점과 달리 bServerInventoryActive 분기가 없다).
	// 막힌 이유는 배너로 알리고 false. 재료가 모자란지 등은 CanCraft로 미리 볼 수 있다
	bool CraftRecipe(const struct FNUCraftRecipeRow& Recipe, int32 Times = 1);

	// 공방 제작 창. 상점처럼 인벤 창을 같이 띄운다 (재료가 가방에 있어야 보인다)
	void OpenCraftUI(int32 ShopId);
	void CloseCraftUI();

	// 이 레시피를 Times번 만들 수 있는지. OutReason이 비어 있지 않으면 그 이유로 막힌다
	bool CanCraft(const struct FNUCraftRecipeRow& Recipe, int32 Times, FText& OutReason) const;

	// 재료·골드·도면으로 몇 번까지 만들 수 있는지 (가방 자리는 안 본다 --- 실제 제작 때 CanCraft가 본다). 상한 99
	int32 GetMaxCraftCount(const struct FNUCraftRecipeRow& Recipe) const;

	// 가방에 있는 그 아이템(NumericId 기준)의 총 개수 --- 제작 재료 표시에 쓴다
	UFUNCTION(BlueprintPure, Category = "NU|Inventory")
	int32 CountItemsByNumericId(int32 NumericId) const;


public:
	UPROPERTY(BlueprintAssignable, Category = "NU|Inventory")
	FOnNUInventoryChanged OnInventoryChanged;

	// 창고(container 2/3) 캐시가 바뀔 때 (인벤 변경은 OnInventoryChanged)
	UPROPERTY(BlueprintAssignable, Category = "NU|Inventory")
	FOnNUStorageChanged OnStorageChanged;

	// --- 패드 집기 (9/16 3단계) --- 집은 칸을 창들이 공유한다 (소지품에서 집어 창고 창에서 놓는다).
	// 창이 닫히거나 마우스로 돌아가거나 마우스 드래그를 시작하면 창이 비운다
	void SetPadHeldItem(int32 Container, int32 SlotNo, const FGuid& InstanceId);
	void ClearPadHeldItem();
	const FNUPadHeldItem& GetPadHeldItem() const { return PadHeldItem; }
	FSimpleMulticastDelegate OnPadHeldItemChanged;

private:
	FNUPadHeldItem PadHeldItem;

	// 그 아이템이 놓일 수 있는 가방 칸 (분류 구간, 구간이 없으면 가방 전체)
	FNUBagSection GetBagRange(FName ItemRowName) const;

	// 그 아이템이 놓일 수 있는 칸 중 비어 있는 가장 앞 (없으면 INDEX_NONE)
	int32 FindFreeSlotNo(FName ItemRowName) const;

	// 범위 안의 빈 가방 칸 수
	int32 CountFreeSlots(const FNUBagSection& Range) const;
	FNUItemInstance* FindMutableBySlotNo(int32 SlotNo);

	// --- 서버 연동 내부 ---
	class UEmberServerManager* GetServerManager() const;
	class UNUBattleNetSubsystem* GetBattleNet() const;

	// 한 칸 반영 (브로드캐스트 없음). Container로 인벤/창고를 구분한다 --- 인벤이면 FastArray를
	// 마킹하고, 어느 쪽이든 모르는 아이템이 오면 그 저장소의 정렬을 막는다
	void ApplySlotToArray(TArray<FNUItemInstance>& Items, int32 SlotNo, int32 ItemId, int32 ItemCount, int32 Container);

	// 인벤 목록 응답의 구간 표(Sections)를 BagSections·BagCategoryByType에 옮긴다. 네 분류가 다 오지 않으면 한 통으로 둔다
	void ReadBagSections(const FJsonObject& Root);

	// 장비 저장소(컨테이너 4) 목록을 장비 컴포넌트에 통째로 반영. 응답에 없는 칸은 빈 부위로 본다
	void ApplyEquipmentSlotsFromJson(const TArray<TSharedPtr<FJsonValue>>* JsonSlots);

	class UNUEquipmentComponent* GetEquipmentComponent() const;

	UFUNCTION()
	void HandleEmberStateChanged(EEmberConnState NewState);

	// 서버 목록에 아이템이 하나라도 오면 서버가 원천 (이동을 서버로 보낸다).
	// 아직 아이템 획득 패킷이 없어(62~69 예약) 빈 목록이면 로컬 개발 흐름을 유지한다
	bool bServerInventoryActive = false;

	// 보낸 구매 요청의 상점 NPC --- 응답이 보낸 순서대로 오므로 앞에서부터 꺼내 퀘스트 목표(PurchaseItem)를 알린다
	TArray<int32> PendingShopBuyNpcIds;

	// 목록을 요청한 저장소 순서. 실패 응답에는 어느 저장소인지가 안 실려 오는데,
	// 인벤(1)·장비(4)는 로그인 때 자동으로 받는 것이라 실패해도 배너를 띄우면 안 된다.
	// 스트림이 요청 순서대로 답하므로 앞에서부터 꺼내 맞춘다
	TArray<int32> PendingListContainers;

	// 클라 테이블이 모르는 아이템이 든 칸: 저장소 => (칸 번호 => 서버가 준 아이템 번호).
	// 그 칸은 화면에 안 그려져 비어 보이므로, 이 상태로 재배치를 보내면 서버 총량 검증에 걸린다.
	// 그래서 해당 저장소의 정렬을 막고, 안내에 칸과 번호를 실어 원인을 알 수 있게 한다.
	// 칸 단위로 들고 있어야 그 아이템이 빠지거나 아는 아이템으로 바뀔 때 저절로 풀린다
	TMap<int32, TMap<int32, int32>> UnknownSlotsByContainer;

	// --- 창고 캐시 (복제 없음 — 각 클라가 자기 gRPC 세션에서 받는다) ---
	struct FNUContainerCache
	{
		int32 SlotCount = 0;
		bool bLoaded = false;
		TArray<FNUItemInstance> Items;
	};
	TMap<int32, FNUContainerCache> StorageCaches; // key: 2(캐릭터 창고), 3(계정 창고)

	// 정의 테이블 둘을 훑어 숫자 id로 행을 찾는다 — 위 공개 함수 둘이 같은 순회를 쓰도록 한 곳에 둔다
	const FNUItemDefRow* FindDefRowByNumericId(int32 NumericId, FName& OutRowName) const;

	// OutReason = 플레이어에게 알릴 거절 사유 (전투불능·전투 중). 그 밖의 거절은 비워 둔다
	bool ValidateUseItem(const FGuid& InstanceId, const FNUItemInstance*& OutInstance, const FNUItemDefRow*& OutDef, FText& OutReason) const;
	bool ApplyUseEffect(const FNUItemDefRow& Def);
	void BroadcastChangedAndPush();
	void EnsureItemDefTable();

	// --- 정렬 ---
	// FailReason을 주면 중단 안내에 사유를 붙인다 (비우면 사유 없이 "중단되었습니다")
	void FinishSort(bool bCompleted, const FText& FailReason = FText());

	// 서버 없이 도는 개발 경로 — 목표 배치를 그대로 로컬 배열에 적용
	void ApplySortLocally(const TArray<FNUItemInstance>& Sorted);

	void HandleSortTimeout();

	// 목표 배치(정렬 키 적용 사본, 칸 번호까지 매긴다). 정의가 없는 아이템은 맨 뒤, 등급 순이면 등급 없는 것도 뒤.
	// 가방에 분류 구간이 있으면 아이템마다 제 구간 앞부터 채운다 --- 구간이 넘치면 false
	bool BuildSortedItems(int32 Container, ENUItemSortMode Mode, TArray<FNUItemInstance>& OutSorted) const;

	class UNUTradeSubsystem* GetTradeSubsystem() const;

	// 서버에 배치를 보내고 응답을 기다리는 동안 참 — 정렬 버튼 연타를 막는다
	bool bSorting = false;

	FTimerHandle SortTimeoutTimer; // 응답 유실 대비 --- 넘기면 중단

	// 지금 정렬 중인 저장소. 한 번에 하나만 보낸다 (응답에 저장소 번호가 없어 구분이 안 된다)
	int32 SortingContainer = INDEX_NONE;

	const UDataTable* GetItemDefTable() const;

	// 창 공용 토글 (1회 생성 캐시) — 열림/닫힘·커서는 UNUUIManager, OnCloseRequested = ESC 시 부를 토글.
	// bCompanion = 다른 창과 함께 뜨는 동반 창 (FNUUIWindowParams::bCompanion, 열 때만 쓴다)
	void ToggleSheetWidget(TSubclassOf<UNUSheetWidgetBase> WidgetClass, TObjectPtr<UNUSheetWidgetBase>& WidgetRef, FSimpleDelegate OnCloseRequested, bool bCompanion = false,
		bool bHideFieldHUD = false, bool bBlockGameInput = false);

	// 통합 캐릭터 메뉴(프로젝트 세팅 NU UI의 CharacterMenuClass)를 그 페이지로 연다.
	// 떠 있으면 같은 페이지일 때 닫고 다른 페이지면 그 페이지로 넘긴다 (bCloseFromAnyPage면 어느 페이지든 닫는다).
	// 셸이 미지정이면 false --- 호출부가 옛 창을 연다. 전투 중에는 열지 않고 키를 받아 버린 것으로 true (옛 창도 안 뜬다)
	bool ToggleCharacterMenu(ENUCharacterMenuSection Section, bool bCloseFromAnyPage = false);

	// 혼자 여는 소지품 자리에 옛 소지품 창을 써야 하는지 --- 다른 창 옆에서 주고받을 때(창고·상점·공방이 연 것, 거래 중)와 옛 창이 떠 있을 때
	bool UsesLegacyInventoryWindow() const;
	// 전투 여부를 다시 읽어, 바뀌었으면 OnOwnerBattleChanged 를 울린다 (전투가 시작된 쪽이면 열린 창부터 전부 닫는다).
	// 서버 전투 신호는 턴마다 오고 끝날 때 여러 번 오므로 값이 바뀔 때만 움직인다
	void RefreshOwnerBattleState();
	bool bOwnerInBattle = false;
#if !UE_BUILD_SHIPPING
	bool bDebugFakeNetBattle = false;
#endif
	APlayerController* GetLocalPlayerController() const;

	UPROPERTY(Replicated)
	FNUInventoryList InventoryList;

	// 기획 데이터 테이블 바인딩 지점 — 지정하면 런타임 더미는 완전히 무시된다 (혼합 금지).
	// ItemDefTable = 소모품(기획 시트/서버 공용 ItemTable.csv), EquipDefTable = 장비(EquipTable.csv — 클라 전용 분리).
	// 조회는 두 테이블 순차 검색 (RowName 중복 금지)
	UPROPERTY(EditDefaultsOnly, Category = "NU|Inventory")
	TObjectPtr<UDataTable> ItemDefTable;

	UPROPERTY(EditDefaultsOnly, Category = "NU|Inventory")
	TObjectPtr<UDataTable> EquipDefTable;

	// 가방 칸 수. 스택 하나가 한 칸(포션 99개=1칸, 100개=2칸).
	// 서버가 붙으면 InventoryList의 SlotCount로 덮인다 — 이 값은 서버 없이 도는 개발 경로의 기본값이고,
	// 기획 공용 상수 시트의 InventorySlotDefault(30)와 맞춘다. 분류 구간이 있으면 구간 칸 수의 합
	UPROPERTY(EditAnywhere, Category = "NU|Inventory", meta = (ClampMin = 1))
	int32 MaxSlots = 30;

	// 분류 구간 (번호 = NUBagCategory). 비어 있으면 가방 전체(0..MaxSlots-1)가 한 통
	TArray<FNUBagSection> BagSections;

	// 서버가 구간과 함께 준 분류 표 (아이템 종류 => NUBagCategory). 비어 있으면 클라 표를 쓴다
	TMap<ENUItemType, int32> BagCategoryByType;

	// 개발용 시작 아이템 (RowName => 개수). PIE 시작 시 서버에서 자동 지급 — Shipping에서는 무시
	UPROPERTY(EditAnywhere, Category = "NU|Inventory|Debug")
	TMap<FName, int32> DebugStartingItems;

	// 런타임 더미 정의 테이블 (GC 보호용 참조 — 구 BattleManager의 런타임 테이블 선례)
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> RuntimeItemDefTable;

	// 인벤토리 창(I키) / 캐릭터 정보창(P키) WBP — PlayerState BP에서 지정 (로아식 분리 창)
	UPROPERTY(EditAnywhere, Category = "NU|Inventory|UI")
	TSubclassOf<UNUInventoryWindowWidget> InventoryWindowWidgetClass;

	UPROPERTY(EditAnywhere, Category = "NU|Inventory|UI")
	TSubclassOf<UNUCharacterInfoWidget> CharacterInfoWidgetClass;

	// 장비창 WBP — 미지정이면 인벤을 열어도 장비창이 뜨지 않는다 (경고만)
	UPROPERTY(EditAnywhere, Category = "NU|Inventory|UI")
	TSubclassOf<UNUEquipmentWidget> EquipmentWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UNUSheetWidgetBase> InventoryWindowWidget;

	UPROPERTY(Transient)
	TObjectPtr<UNUSheetWidgetBase> CharacterInfoWidget;

	UPROPERTY(Transient)
	TObjectPtr<UNUSheetWidgetBase> EquipmentWidget;

	// 창고 창. 클래스는 프로젝트 세팅(NU UI)의 StorageWindowClass (WBP_StorageWindow)
	UPROPERTY(Transient)
	TObjectPtr<UNUSheetWidgetBase> StorageWindowWidget;

	// 제작 창 (공방). 클래스는 프로젝트 세팅(NU Shop)의 CraftWindowClass
	UPROPERTY(Transient)
	TObjectPtr<UNUSheetWidgetBase> CraftWindow;

	// 창고를 열면서 인벤 창을 같이 켰는지 — 닫을 때 같이 닫아주기 위한 표시
	// (원래 열어둔 인벤이었다면 창고만 닫는다)
	bool bInventoryOpenedByStorage = false;

	// 상점 창. 클래스는 프로젝트 세팅(NU Shop)의 ShopWindowClass --- PlayerState BP마다 지정하지 않아 공용 테스트 맵에서도 열린다
	UPROPERTY(Transient)
	TObjectPtr<UNUSheetWidgetBase> ShopWindow;

	// 상점이 열면서 켠 인벤인지 — 창고와 같은 규칙 (닫을 때 같이 닫는다)
	bool bInventoryOpenedByShop = false;

	UFUNCTION()
	void OnRep_Gold();

	// UE 서버에서는 소유자에게 복제하며 외부 서버에서는 수신 잔액을 적용한다.
	UPROPERTY(ReplicatedUsing = OnRep_Gold, EditAnywhere, Category = "NU|Shop")
	int64 Gold = 10000;
};
